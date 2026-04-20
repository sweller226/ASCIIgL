# M4 — Variant compiler → state IDs

This document is the **implementation specification** for milestone **M4** in `MODEL_LOADING_PLAN.md`: wiring `BlockStateRegistry` flattened `stateId`s to Minecraft 1.8.9 `blockstates/*.json` `variants` entries, then resolve + bake + register into `BlockModelLibrary`.

**Status:** specification with per-phase notes. **Phases 1–5** are **done** in code + `Game::InitializeBlockStates`; **Phase 6** is a **manual checklist** (no init-time verifier). **Phase 3** is **done**: variant arrays are preserved and selected deterministically with equal probability (no numeric `weight` parsing).

**Related:** `MODEL_LOADING_PLAN.md` (milestones M0–M7), architecture section *How Blockstate JSON Fits Your Architecture*.

---

## Goal

For every `stateId` that should be JSON-backed:

1. Build a **variant key string** that matches the keys in `variants { ... }` for that block’s blockstate file.
2. Look up the key → obtain `VariantModelRef` (model id, `x`, `y`, `uvlock`).
3. **Resolve** the model (`ResolveBlockModel`), **bake** it (`BakeResolvedModel` with variant transforms), **register** (`BlockModelLibrary::RegisterModel`).

**Deliverable:** automatic per-state model registration from 1.8.9 assets for chosen block types, with deterministic fallbacks and controlled logging.

---

## Preconditions (already in tree)

| Piece | Role |
|--------|------|
| `blockstate::BlockStateRegistry` | `typeId`, property defs, `stateId`, `GetPropertyValue`, `WithProperty`, `Resolve` |
| `blockstate::JsonBlockStateLoader` | Parsed `BlockstateDefinition` (`variants` → `vector<VariantModelRef>` per key) |
| `blockmodels::JsonModelLoader` | Parsed block models on disk |
| `blockmodels::ResolveBlockModel` | Parent + `#` texture resolution → `ResolvedBlockModelDefinition` |
| `blockmodels::BakeResolvedModel` | Geometry + variant `x`/`y`/`uvlock` → `blockstate::BlockModel` |
| `blockmodels::BlockModelLibrary` | `stateId` → shared `BlockModel` |
| `blocktextures::GetLayerForTextureId` | Texture id → terrain array layer (catalog + canonicalization) |

Assets root (today): e.g. `res/textures/1.8.9` — same root passed to both loaders.

---

## Architecture contract (do not break)

- Runtime stays **baked-model based**: chunk meshing reads `BlockModelLibrary`, not JSON.
- JSON work runs at **init** (or explicit reload), not inside per-chunk hot paths.
- **Hybrid** remains valid: programmatic registration (e.g. water) alongside JSON-backed types (M6 will formalize per-type source).

---

## End-to-end data flow

1. Register block **type** in `BlockStateRegistry` with `BlockProperty` rows whose **names and allowed value strings** match vanilla (e.g. `"north"` → `"true"` / `"false"`).
2. For that type, use the **registry block type name** as the blockstate resource id (vanilla: `minecraft:stone` → `assets/minecraft/blockstates/stone.json`). No extra mapping layer unless you add non-vanilla ids later.
3. `JsonBlockStateLoader::GetOrLoadBlockstate(...)` → `BlockstateDefinition`.
4. For each `stateId` in `[baseStateId, baseStateId + stateCount)`:
   - Emit **variant key** string.
   - `variants.find(key)` (with fallbacks; see below).
   - Parser keeps all entries when the JSON value is a variant array (equal-probability set; no numeric `weight` field).
   - `ResolveBlockModel` + `BakeResolvedModel` + `RegisterModel(stateId, model, bsr)`.

---

## Phase 1 — Variant key representation

**Implemented:** `VariantKey.hpp` / `VariantKey.cpp` — `BuildVariantKey`, `AssertUniqueVariantKeysPerType` (called from `Game::InitializeBlockStates` for all types).

### 1.1 Parse variant keys (optional but useful)

Input: string like `east=true,north=false,south=true,west=false`.

- Split on `,`.
- Each segment `name=value` → map entry.
- Use for **diagnostics** and optional canonical reorder experiments — **lookup must match JSON keys as authored** unless you prove a single canonical ordering for every block you support.

### 1.2 Build variant key from `stateId`

Primary strategy (recommended for M4):

- Iterate `BlockType::properties` in **registration order** (stable).
- For each property `p`, append `p.name + "=" + GetPropertyValue(stateId, p.name)`.
- Join with `,` (no spaces), matching vanilla 1.8.9 convention for your first target blocks.

**Important:** values must be **strings** exactly as in JSON keys (`"true"` / `"false"`, not `1`/`0`). Keep using `BlockProperty::allowedValues` with vanilla spelling; do **not** introduce a parallel boolean type in the registry for M4 (see *Risks*).

### 1.3 Uniqueness check (debug / init)

For each JSON-backed type, optionally assert that every `stateId` in the type produces a **unique** key string among states of that type. Collisions usually mean property ordering or value strings do not match how you registered the type.

---

## Phase 2 — Blockstate dispatch table

**Status: done** — resource id alignment, variant lookup fallbacks, and model id handoff to `ResolveBlockModel` are implemented (`JsonBlockModelRegistration.cpp` / `JsonBlockStateLoader`).

### 2.1 Resource id

Pass the same string you use in `BlockStateRegistry::RegisterType` (e.g. `minecraft:stone`) to `JsonBlockStateLoader::GetOrLoadBlockstate`. That is the blockstate id; `ResourceLocation::Parse` + `ResolveBlockstatePath` already map it to `assets/<ns>/blockstates/<path>.json`. Add a dedicated remap only if a type id and asset file ever diverge.

### 2.2 Lookup

**Implemented:** `JsonBlockModelRegistration.cpp` (`LookupVariantModelRef`).

- `BlockstateDefinition::variants` is `unordered_map<string, VariantModelRef>`.
- Lookup order (hardcoded in registrar):
  1. **Exact** key from Phase 1.
  2. If miss: try **`""`** empty key (vanilla default variant for many simple blocks).
  3. If still miss: try **`"normal"`** (common in 1.8.9 when the engine’s built key is empty but JSON uses `normal`).
  4. On final miss: **`Logger::Warning`** with type, built key, and path hint (may repeat per `stateId`).

### 2.3 Model id from `VariantModelRef`

**Implemented:** same registrar — `ref.model.ToString()` is passed to `ResolveBlockModel`.

- `JsonBlockStateLoader` already normalizes short `model` strings (e.g. `dandelion` → `minecraft:block/dandelion`).
- Pass `ref.model.ToString()` (or equivalent) into `ResolveBlockModel`.

---

## Phase 3 — Variant arrays (equal-probability)

**Status: done** — variant arrays are parsed as full entry lists. Selection is deterministic by world position (`stateId + world xyz`) with equal probability across entries. Numeric `weight` is intentionally not parsed or used.

---

## Phase 4 — Registration pipeline (suggested API)

**Status: done** — `blockstate::RegisterJsonBackedBlockType(...)` in `JsonBlockModelRegistration.hpp` / `.cpp` (type must already be on `BlockStateRegistry` after `RegisterType` + `SetDerivedData`; variant fallbacks `""` / `"normal"` live in the registrar).

**Internals (per type):**

1. Load blockstate JSON for that type (`JsonBlockStateLoader::GetOrLoadBlockstate`).
2. Caller supplies shared `JsonModelLoader` + `JsonBlockStateLoader` for the same `assetsRootPath` (see `Game::InitializeBlockStates`).
3. For each `stateId` in the type’s range:
   - Build key → lookup → pick `VariantModelRef`.
   - `ResolveBlockModel(modelLoader, modelIdString)`.
   - `BakeResolvedModel(resolved, ref.x, ref.y, ref.uvlock)` — baker uses `blocktextures::GetLayerForTextureId` for layers.
   - `RegisterModel(stateId, std::make_shared<const BlockModel>(std::move(baked)), bsr)` on success.

On variant miss or resolve/bake failure: log that state; **do not** register a model for that `stateId`; return `false` from `RegisterJsonBackedBlockType` if any state in the type failed (milestone treats partial failure as init error). The code does not register `nullptr` models.

---

## Phase 5 — Integration order in `Game::InitializeBlockStates`

**Status: done** — `Game::InitializeBlockStates` (see `Game.cpp`):

1. **Air** — first type; non-renderable; `nullptr` model.
2. **Shared loaders** — one `assetsRootPath` (`res/textures/1.8.9`): `JsonBlockStateLoader` + `JsonModelLoader` constructed immediately after air.
3. **Programmatic / cube types** — bedrock, grass, water, cubes, etc., interleaved with JSON-backed types as needed for stable `RegisterType` order.
4. **JSON-backed rollout** — `registerJsonBackedOrLog` calls `RegisterJsonBackedBlockType` for `minecraft:stone`, `minecraft:dandelion`, and `minecraft:fence` (1.8.9 oak fence uses blockstate id `fence.json` → type `minecraft:fence`).
5. **Exit criteria** — manual or external tests (see checklist below); startup does not run an automated M4 verification pass.

---

## Phase 6 — Verification / exit criteria (M4)

**Status: documented** — use when validating JSON rollout (not run each launch):

- **Stone** / **dandelion**: all states of each type have a registered model.
- **Fence** (`minecraft:fence`): spot-check `Resolve` + `BuildVariantKey` against `assets/minecraft/blockstates/fence.json`; each checked `stateId` has a model.
- **Global**: every `BlockState::isRenderable` state has a non-null model before relying on chunk meshing.

---

## Suggested file layout (under reorganized `world/`)

| Piece | Suggested location |
|--------|---------------------|
| Parse / build variant key | `include/ASCIICraft/world/block/state/VariantKey.hpp` + `src/.../VariantKey.cpp` |
| Orchestration: blockstate → resolve → bake → register | `JsonBlockModelRegistration.hpp` + `JsonBlockModelRegistration.cpp` |

Keep `JsonBlockStateLoader` and `JsonModelLoader` as **load/parse** only; M4 adds the **compiler** layer that talks to `BlockStateRegistry` + `BlockModelLibrary`.

---

## Property and registry policy

- **Strings only** in `BlockProperty` for values that appear in variant keys (`"true"`, `"false"`, `"north"`, …).
- Optional C++ helpers (`IsConnectionTrue(bsr, stateId, "north")`) can parse strings — do not replace the registry’s string storage for M4.

---

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Key string mismatch (spacing, order, `true` vs `True`) | Build key from registry in one place; compare against real JSON for each new block type. |
| Engine property names ≠ JSON (`facing` vs typo) | Per-type registration review; init-time assert optional. |
| Missing texture in catalog | Bake fails — extend catalog or narrow JSON rollout. |
| Startup cost | Reuse `BlockModelLibrary` dedup by fingerprint; cache resolved models if needed later. |

---

## Rollout order (recommended)

1. **Stone** — validates `""` default variant + simple cube parent chain.
2. **Dandelion** — validates `block/cross` + texture path + catalog.
3. **Oak fence** — validates multi-property keys; full correctness waits on **M5** neighbor updates for all 16 patterns.

---

## Next milestone (M5)

M4 only **registers** models for whatever `stateId` the world already uses. **Neighbor-driven** updates (`north`/`east`/…) so the world actually visits every fence variant are **M5** (*Connected Block State Updater* in `MODEL_LOADING_PLAN.md`).

---

## Document history

- Authored as the working M4 implementation spec derived from the project’s `MODEL_LOADING_PLAN.md` M4 bullet list and the current `world/block/...` include layout.
