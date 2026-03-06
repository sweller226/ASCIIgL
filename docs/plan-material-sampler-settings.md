# Plan: Texture Sampling Settings on Material

Add per-slot sampler/filter choice to the Material class so the material (not the resource type) decides whether to use point or anisotropic sampling. Backward compatible via a "Default" option that keeps current behavior.

---

## 1. Goal

- **Material** owns the sampling choice per texture slot (Point vs Anisotropic).
- **Renderer** uses that choice when binding the material; when binding a texture/array directly (no material), behavior stays as today (texture → point, array → anisotropic).
- **Default**: If a slot does not set a sampler, use "Default" = renderer infers (texture → point, array → anisotropic) so existing code is unchanged.

---

## 2. Sampler Type Vocabulary

Introduce a small shared enum so both Material and Renderer use the same names. Avoid putting it only in Material.hpp so Renderer.hpp doesn’t need to include all of Material.

**Option A (recommended):** New header `include/ASCIIgL/renderer/SamplerType.hpp`:

```cpp
#pragma once

namespace ASCIIgL {

enum class SamplerType {
    Default,    // Renderer chooses: Texture → Point, TextureArray → Anisotropic
    Point,      // Point + linear mip (pixel-art, GUI)
    Anisotropic // Anisotropic (terrain, 3D)
};

} // namespace ASCIIgL
```

- **Material.hpp** and **Renderer.hpp** (or Renderer.cpp) include `SamplerType.hpp`.
- **Default**: Keeps current behavior; no API change required for existing materials.
- **Point**: Force point-style sampling (current `_samplerLinear`).
- **Anisotropic**: Force anisotropic (current `_samplerAnisotropic`).

Later you can add `Linear` (trilinear, if you add a third sampler) without changing this plan.

**Option B:** Define `SamplerType` in `Material.hpp` and have `Renderer.hpp` include `Material.hpp` only for the enum. Simpler file count, but heavier include for Renderer. Acceptable if you prefer fewer files.

---

## 3. TextureSlot and Material Changes

**File: `include/ASCIIgL/renderer/Material.hpp`**

- Include `SamplerType.hpp` (or define `SamplerType` here if Option B).
- Extend **TextureSlot**:
  - Add: `SamplerType samplerType = SamplerType::Default;`
  - Constructor: either leave default or add an optional `SamplerType` parameter to `TextureSlot(...)` (e.g. default `SamplerType::Default`).
- **Material** API:
  - `void SetSamplerForSlot(uint32_t slot, SamplerType type);`
  - `void SetSamplerForSlot(const std::string& name, SamplerType type);`
  - `SamplerType GetSamplerForSlot(uint32_t slot) const;` (return `Default` if slot missing)
- **AddTextureSlot**: Optionally allow `AddTextureSlot(name, slot, defaultSampler)`; if not added, new slots get `SamplerType::Default`.
- **Clone**: When cloning a material, copy `samplerType` for each `TextureSlot` (clone already copies `_textureSlots`; ensure the new struct member is copied).

**File: `src/renderer/Material.cpp`**

- Implement `SetSamplerForSlot(slot, type)` and `SetSamplerForSlot(name, type)` by finding the slot and setting `slot.samplerType = type`.
- Implement `GetSamplerForSlot(slot)` by finding the slot and returning `slot.samplerType`, or `SamplerType::Default` if not found.
- In `SetTexture` / `SetTextureArray` (by slot or name), do **not** reset `samplerType` unless you explicitly want “set texture and reset sampler to Default” (recommendation: leave sampler as-is when only setting texture).
- In `Clone()`, ensure the copied `_textureSlots` include the new field (copy by value is enough if `TextureSlot` has `samplerType`).

---

## 4. Renderer Changes

**Resolving Default**

- When the renderer needs a concrete sampler for a slot:
  - If `SamplerType::Point` → use `_samplerLinear`.
  - If `SamplerType::Anisotropic` → use `_samplerAnisotropic`.
  - If `SamplerType::Default` and resource is a **Texture** → use `_samplerLinear`.
  - If `SamplerType::Default` and resource is a **TextureArray** → use `_samplerAnisotropic`.

So “Default” preserves current behavior.

**Where to resolve**

- **Option A:** Extend **BindTexture** and **BindTextureArray** with an optional parameter:
  - `void BindTexture(const Texture* tex, int slot, SamplerType type = SamplerType::Default);`
  - `void BindTextureArray(const TextureArray* texArray, int slot, SamplerType type = SamplerType::Default);`
  - Each function sets the SRV as now, then sets the sampler for that slot using the resolution above (Default → infer from resource type).
- **Option B:** Keep `BindTexture(tex, slot)` and `BindTextureArray(array, slot)` unchanged. Only in **BindMaterial** pass the slot’s `samplerType` into an internal bind helper that sets both SRV and sampler. Public bind API stays the same; only material-driven binding uses the new setting.

Recommendation: **Option A** so that any code that binds a texture/array can optionally pass a sampler type; **BindMaterial** then calls `BindTexture(slot.texture, slot.slot, slot.samplerType)` and `BindTextureArray(slot.textureArray, slot.slot, slot.samplerType)`.

**Files to touch**

- **Renderer.hpp**
  - Include `SamplerType.hpp` (or Material.hpp if enum lives there).
  - Declare:
    - `void BindTexture(const Texture* tex, int slot = 0, SamplerType type = SamplerType::Default);`
    - `void BindTextureArray(const TextureArray* texArray, int slot = 0, SamplerType type = SamplerType::Default);`
- **Renderer_Textures.cpp**
  - Implement the new signatures:
    - Resolve `type`: if Default and binding texture → Point; if Default and binding array → Anisotropic; else use `type`.
    - After setting the SRV, call `PSSetSamplers(slot, 1, &resolvedSampler)` (either `_samplerLinear` or `_samplerAnisotropic`).
  - Existing call sites that use `BindTexture(tex, slot)` or `BindTextureArray(array, slot)` get default behavior (no change).
- **Renderer.cpp (BindMaterial)**
  - In the loop over `material->_textureSlots`:
    - `if (slot.texture) BindTexture(slot.texture, slot.slot, slot.samplerType);`
    - `else if (slot.textureArray) BindTextureArray(slot.textureArray, slot.slot, slot.samplerType);`

---

## 5. Summary of Files

| File | Action |
|------|--------|
| **New: `include/ASCIIgL/renderer/SamplerType.hpp`** | Define `enum class SamplerType { Default, Point, Anisotropic }`. |
| **Material.hpp** | Include SamplerType.hpp; add `samplerType` to `TextureSlot`; add `SetSamplerForSlot`, `GetSamplerForSlot`. |
| **Material.cpp** | Implement Set/GetSamplerForSlot; ensure Clone copies `samplerType`. |
| **Renderer.hpp** | Include SamplerType.hpp; add optional `SamplerType` parameter to `BindTexture` and `BindTextureArray`. |
| **Renderer_Textures.cpp** | Implement Default resolution and set sampler in BindTexture/BindTextureArray based on `type`. |
| **Renderer.cpp** | In BindMaterial, pass `slot.samplerType` into BindTexture/BindTextureArray. |

---

## 6. Usage Example

```cpp
// Existing: unchanged (Default → texture gets point, array gets anisotropic)
blockMaterial->SetTextureArray(0, blockTextureArray.get());

// Explicit: terrain with anisotropic (redundant with Default for array, but clear)
blockMaterial->SetTextureArray(0, blockTextureArray.get());
blockMaterial->SetSamplerForSlot(0, SamplerType::Anisotropic);

// Force point for a texture array (e.g. inventory grid)
inventoryMaterial->SetTextureArray(0, terrainArray.get());
inventoryMaterial->SetSamplerForSlot(0, SamplerType::Point);

// GUI texture: point (default for single texture)
guiMaterial->SetTexture(0, guiTexture);
// guiMaterial->SetSamplerForSlot(0, SamplerType::Point); // optional, Default is already Point
```

---

## 7. Backward Compatibility

- All existing `SetTexture` / `SetTextureArray` calls leave `samplerType` as **Default** (or it is the default value for new slots).
- Default resolution matches current behavior (texture → point, array → anisotropic).
- No change required in ASCIICraft or other games until you want to override a slot’s sampler.

---

## 8. Optional Follow-ups

- **Linear**: Add `SamplerType::Linear` and a third D3D11 sampler (trilinear), then resolve it in the same way.
- **Per-texture default**: Later, textures could carry a “default sampler” hint; when material slot is Default, use texture’s hint instead of inferring from texture vs array.
- **Anisotropy level**: If you want materials to override the global anisotropy (e.g. 4x vs 16x), you could add a separate setting (e.g. per-slot anisotropy level) and have the renderer create or select samplers accordingly; that can be a separate plan.
