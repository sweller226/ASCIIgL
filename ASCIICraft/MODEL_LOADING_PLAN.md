# Minecraft 1.8.9 Model Loading Plan

## Goal

Load and render Minecraft-style custom block models (fences, fence gates, torches, flowers, doors, etc.) from 1.8.9 JSON assets, while fitting the current runtime contract:

- `BlockStateRegistry` produces `stateId`
- `BlockModelLibrary` maps `stateId -> BlockModel`
- `ChunkMeshGen` consumes baked `BlockModel`

---

## Target Version Decision

Use **1.8.9 first**.

Why:

- 1.8.9 uses explicit `variants` mapping, which fits your existing `stateId -> model` architecture.
- Avoids introducing multipart composition complexity immediately.
- You can still support connected/custom shapes using explicit variant combinations.

Defer 1.9+ `multipart` as a later phase.

---

## Key Requirements Confirmed by 1.8.9 Assets

From broad sampling across many block model and blockstate files:

- Parent inheritance + texture overrides are pervasive.
- Texture indirection chains (`#name -> #other -> path`) are common.
- Models frequently use:
  - per-face `uv`
  - per-face `rotation`
  - per-face `cullface`
  - element-level `rotation` (`origin`, `axis`, `angle`, optional `rescale`)
  - optional `ambientocclusion` overrides
  - optional `tintindex`
- Variant JSON entries commonly use `x/y` transforms and `uvlock`.
- 1.8.9 blockstates are predominantly explicit `variants` mappings.
- Variant arrays for randomized selections appear in blockstates (weight support should be part of the design, with default weight handling).
- Item rendering depends on `models/item` and `display` transforms (separate from world block meshing, but part of the full asset pipeline).

General engine rules:

1. Parent + texture variable resolution is mandatory for all JSON-backed models.
2. Variant-key canonicalization must be strict and deterministic for all state lookups.
3. Variant transforms (`x/y`) and `uvlock` are core model dispatch features and must be treated as global MVP requirements, not block-specific exceptions.
4. Connected-property state updates are required wherever world-neighbor connectivity drives blockstate variants (fences, panes, walls, gates, etc.).
5. Geometry bake must support full 1.8.9 element/face schema, not just cuboid-with-basic-uv subsets.

---

## Architecture Contract (Do Not Break)

- Runtime remains baked-model based (`BlockModelLibrary`).
- JSON loading runs at init/reload time, not in chunk meshing hot-path.
- Programmatic builders remain supported for special blocks (e.g., water).
- Hybrid source model is allowed:
  - Programmatic for selected types
  - JSON-backed for custom/complex blocks

---

## How Blockstate JSON Fits Your Architecture

`blockstates/*.json` is the dispatch layer, and `models/block/*.json` is the geometry layer.

For your current runtime (`stateId -> BlockModel`), integration should be:

1. `BlockStateRegistry` defines block types and property sets (already true).
2. For a given block type, load `assets/<ns>/blockstates/<block>.json`.
3. For each concrete engine state (`stateId`), build a canonical variant key (`prop=value,...`).
4. Lookup that key in blockstate JSON `variants`.
5. Resolve selected model ref + variant transforms (`x`, `y`, `uvlock`).
6. Load/resolve model JSON (`models/block/...`), bake to `BlockModel`.
7. Register baked model into `BlockModelLibrary` by `stateId`.

This means blockstate JSON is required for blocks like fences, stairs, doors, rails, etc., because the blockstate file defines which model variant corresponds to each property combination.

### Fence Example (`birch_fence.json`)

- Keys like `east=true,north=false,south=true,west=false` are exact variant selectors.
- Values choose the model and transform (e.g., `birch_fence_ne` + `y=90` + `uvlock=true`).
- Your connected-state updater (M5) must keep those four booleans accurate so the correct variant key is produced.

---

## Milestones

## M0 - Contracts + Diagnostics (Done)

- Define bake input/output contracts in code comments/docs.
- Add structured logging for:
  - missing blockstate/model files
  - unresolved parent/texture variable
  - invalid variant key mapping
- Add "warn once" behavior to avoid log spam.

Deliverable: stable diagnostics baseline. (Completed)

## M1 - Resource + JSON Loader Core (Done)

- Implement resource location resolver:
  - `assets/<namespace>/blockstates/*.json`
  - `assets/<namespace>/models/block/*.json`
- Implement typed parsing for:
  - Blockstate variants (1.8 format)
  - Block model (`parent`, `textures`, `elements`, face data)
- Add caches:
  - parsed definitions by resource location

Deliverable: can parse target files and print/inspect internal representation. (Completed)

Note: variant-key parsing/canonicalization for `BlockStateRegistry` → blockstate JSON lives in **M4** (state→variant mapping), not in the loaders.

### Starting M2 (concrete)

1. Introduce a **resolved model** type (e.g. `ResolvedBlockModelDefinition` or bake-time input struct): same fields as parsed JSON but with **no** `parent`, **no** `#` texture references — only concrete texture strings and merged `elements` (and inherited fields applied).
2. Implement **`ResolveBlockModel(assetsRoot, ResourceLocation modelId)`** (or similar) that:
   - loads parsed JSON via `JsonModelLoader` (cache hit),
   - walks `parent` recursively with **cycle detection**,
   - merges `textures` maps child-over-parent,
   - for each face `texture`, resolves `#name` → final path or next `#` hop until literal.
3. Only after resolution is stable, **M3** bakes that resolved structure — do not bake raw parsed JSON with parents still attached.

## M2 - Model Resolver

- Parent-chain resolution with cycle detection.
- Texture variable (`#...`) resolution across inheritance.
- Emit fully resolved model definition for baking.

Deliverable: resolved output for sample models like:
- `fence_post`
- `fence_n`
- `fence_nsew`

Status note: core implementation is in place, but sample-model validation is still pending.

## M3 - Geometry Baker

Goal: convert `ResolvedBlockModelDefinition` into runtime `blockstate::BlockModel` buffers used by `ChunkMeshGen`.

### M3A - Baker Contract + Data Path (Done)
- Add a baker entry point (e.g. `BakeResolvedModel(...)`) in `blockmodels`.
- Input:
  - `ResolvedBlockModelDefinition`
  - variant transform payload (`x`, `y`, `uvlock`) from blockstate variant entry
  - texture lookup callback/lambda (`resourceId -> texture layer index`)
- Output:
  - fully populated `blockstate::BlockModel` (`opaque`/`transparent` layers + face ranges)
- Rule: M3 ignores `tintindex` and `ambientocclusion` for rendering behavior (keep parsed/stored only).

Status: implemented via `BlockModelBaker` contract (`BakeResolvedModel(...)`) with variant transform inputs, texture-layer resolver callback, and `computeVisibleFaces = nullptr` policy for JSON models.

### M3B - Element to Face Expansion (Done)
- For each resolved element:
  - build cube-local face quads from `from/to` (model-space 0..16)
  - convert to block-local 0..1
  - skip missing faces (only emit faces present in JSON)
- For each face:
  - derive UV rectangle (`uv` or inferred default from face orientation)
  - apply face `rotation` (0/90/180/270) to UV mapping
  - resolve texture layer from concrete face `texture`
- Keep `cullface` as metadata for future culling logic (store/propagate where useful).

Status: implemented in `BlockModelBaker` (face emission, default/inferred UVs, face UV rotation, texture-layer resolution).

### M3C - Transform Stack (Done)
- Apply transforms in this order:
  1. element rotation (`origin`, `axis`, `angle`, `rescale`) in model-space
  2. normalize to block-local space
  3. variant rotation (`x`, `y`)
  4. `uvlock` handling for rotated variants
- Keep transformation code in isolated helper functions for easier debugging.

Status: implemented for MVP in `BlockModelBaker` (element rotation + normalize + variant x/y + uvlock handling).

### M3D - Layer Routing + Runtime Flags
- Decide per-face/per-element pipeline routing:
  - default all baked JSON block geometry to opaque for now unless you explicitly tag known transparent blocks in integration step
  - retain ability to route some faces to transparent later
- Set `isFullBlock` conservatively:
  - true only when geometry is exactly full cube equivalent
  - otherwise false
- Set `computeVisibleFaces` policy:
  - if `isFullBlock == true`, assign full-block culling callback (`faceculling::ComputeVisibleFacesFullBlock`)
  - otherwise keep `computeVisibleFaces = nullptr` for JSON models in this milestone

Practical MVP heuristic for `isFullBlock`:
- mark as full block when the resolved parent chain indicates `block/cube_all` (e.g. sponge-like models).
- leave false for all other model families until stricter geometric checks are added.

### M3E - Integration Hook (without full M4 wiring) (Done)
- Add a temporary integration function that:
  - resolves + bakes a fixed set of model ids (`fence_post`, `fence_n`, `fence_nsew`, `torch`, `dandelion`)
  - logs vertex/index counts and any failures
- This is not full stateId mapping yet; it is a baker shakedown pass.

Status: completed and validated using a temporary runtime shakedown helper, then removed to keep startup clean.

### M3F - Exit Criteria (Done)
- Can bake resolved models into valid `blockstate::BlockModel` with stable buffer sizes.
- Variant transforms (`x`, `y`, `uvlock`) visibly affect geometry/UVs in debug output.
- `tintindex` and `ambientocclusion` are intentionally ignored for render output in this milestone.
- No crashes on representative assets (fence, torch, flower, simple cube model).
Status: validated via temporary one-off runtime validator, passed, then removed.

Deliverable: baker produces runtime `BlockModel` buffers from resolved JSON models for representative blocks.

## M4 - Variant Compiler -> State IDs

- Variant-key helpers (for matching blockstate `variants` keys):
  - parse variant key (`a=b,c=d`) into property map
  - re-emit canonical key with stable property order where needed
  - exact-string lookup first; canonical form for mismatches/diagnostics
- For each block type/state in `BlockStateRegistry`:
  - build canonical variant key string (`prop=value,...`)
  - map key to blockstate JSON variant entry
  - resolve model ref + transform
  - bake and register in `BlockModelLibrary`
- Add strict fallback policy when key misses:
  - try exact canonical key
  - optional fallback to empty key `""` (for simple blocks with default variant)
  - emit warn-once for unresolved state->variant mapping with block id + key
- Support variant arrays/weights as first-class behavior:
  - deterministic mode (for reproducibility/debug)
  - weighted selection mode (for visual parity with vanilla behavior)
  - default weight of `1` when omitted

Deliverable: automatic per-state model registration from JSON.

## M5 - Connected Block State Updater (Critical for fences/gates)

- Add neighbor-aware property updates for connection-driven blocks:
  - fence: `north/east/south/west`
  - fence gate: open/in_wall/facing
  - later: panes/walls/etc.
- Trigger updates on:
  - placement
  - removal
  - neighbor changes
- Ensure state transitions produce valid variant keys.
- Explicitly verify fence-style booleans (`north/east/south/west`) match blockstate JSON key names and value format (`true/false`).

Deliverable: world state actually reaches the variant combinations in blockstate JSON.

## M6 - Hybrid Integration in `Game::InitializeBlockStates`

- Keep programmatic registration for selected blocks (e.g., water).
- Add JSON registration path for custom blocks.
- Add per-type source selection:
  - `Programmatic`
  - `Json18`
- Add per-type JSON entry metadata:
  - blockstate resource id (`minecraft:birch_fence`)
  - optional override namespace/path when engine type name differs from asset name

Deliverable: mixed world works with both model sources.

## M6.5 - Item Model Pipeline (Optional but Recommended Early)

- Parse and resolve `models/item` for block items and handheld display.
- Support `display` transform data for GUI/hand/fixed contexts.
- Keep this path logically separate from chunk/world block meshing.

Deliverable: item/block visuals can share resource pipeline without coupling world meshing to item transform logic.

## M7 - Validation + Perf

- Add tests/checks for:
  - parent/texture resolution
  - variant key mapping
  - bake counts/bounds
  - element rotation correctness
  - face rotation correctness
  - uvlock transform correctness
- Visual validation set:
  - fence all 16 connection states
  - fence gate orientations/open states
  - wall torch 4 directions + up
  - flowers (crossed planes)
- Cache baked model outputs to keep startup acceptable.

Deliverable: stable and verifiable rollout.

---

## Post-MVP (Optional)

- Add 1.9+ multipart support:
  - parse `multipart`
  - condition evaluator (`when`, OR/AND)
  - combine multiple baked parts into one runtime model

This should be a separate milestone after 1.8.9 path is proven.

---

## Initial Block Rollout Order

1. Torch
2. Flower / cross-plant
3. Fence
4. Fence gate
5. Door

This order validates transform, uvlock, connectivity, and multi-part-like behavior in increasing complexity.

---

## Risks and Mitigations

- Variant mismatch due to key formatting
  - Mitigation: canonical key builder from `BlockStateRegistry`
- Variant mismatch due to property name divergence (engine vs asset)
  - Mitigation: per-type property alias/validation checks at init
- UV drift between programmatic and JSON models
  - Mitigation: central UV utility and explicit test cases
- Over-culling custom geometry
  - Mitigation: retain `cullface` metadata, use conservative defaults first
- Startup cost from parsing/baking many assets
  - Mitigation: cache resolved and baked models

