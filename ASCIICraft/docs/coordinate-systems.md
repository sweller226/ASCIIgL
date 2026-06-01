# Coordinate System Reference (ASCIIgL / ASCIICraft)

This document describes where `(0, 0)` lives in each layer of the engine and game, how axes point, and where conventions diverge. Use it when adding GUI, textures, world logic, or render passes.

---

## 1. Quick reference

| Space | Origin `(0, 0)` | +X | +Y | +Z | Notes |
|--------|-----------------|-----|-----|-----|--------|
| **Screen / GUI pixels** | Top-left of viewport | Right | Down | N/A (2D) | Same as `Camera2D` ortho |
| **Character grid (`Screen`)** | Top-left cell | Right | Down | — | `GetWidth()` / `GetHeight()` are **cells**, not window pixels |
| **D3D11 texture UV** | Top-left of texel (0,0) | U right | V down | — | Matches PNG row order with `stbi_set_flip_vertically_on_load(0)` |
| **World blocks (`WorldCoord`)** | Arbitrary world point | East (+X) | Up (+Y) | See §4 — **Z naming is mixed** | Integer block indices |
| **Block model JSON (0–16)** | Corner of element | +X | +Y | +Z | Scaled by `1/16` to block space |
| **Chunk local (0–15)** | Chunk corner `(0,0,0)` | +X | +Y | +Z | `GetBlockIndex(x,y,z) = x + y*S + z*S²` |
| **3D camera (`Camera3D`)** | Camera position | Right (world) | Up (world) | Forward via yaw/pitch | Y-up world, GLM `lookAt` |
| **GUI quad local mesh** | Center of unit square | +X right | +Y up **in mesh space** | — | Mapped to screen Y-down via `Camera2D` + scale |

---

## 2. Screen and 2D rendering

### 2.1 Logical screen (`ASCIIgL::Screen`)

- **`Screen::GetWidth()` / `GetHeight()`** return the **character cell grid** size (e.g. 80×50), not physical pixels.
- The GPU color RT, depth buffer, viewport, and quantization pass all use this **cell count** as width/height.
- **Terminal mode:** `_pixelBuffer[y * width + x]` — `(0,0)` is top-left; +Y is down (`ScreenTerminalImpl::PlotPixel`).
- **Window mode:** logical size is still cells; window pixel size = `cells × cellPixels` (`ScreenWindowImpl`).

### 2.2 `Camera2D` (GUI / 2D ortho)

```cpp
proj = glm::ortho(0.0f, w, h, 0.0f, -100.0f, 100.0f);
```

- **Origin:** top-left `(0, 0)`.
- **+X:** right, **+Y:** down (matches Windows / PNG / GUI layout).
- **View:** translation by `-position` (default position `(0,0)` → no offset).
- **Units:** one unit = one **screen cell** (same as GUI pixel coords when the HUD uses cell dimensions).

**Source:** `ASCIIgL/src/engine/Camera2D.cpp`

### 2.3 GUI widgets (`gui::Widget`)

- **`screenPosition`:** always the widget’s **top-left** in screen pixels/cells.
- **`anchor` / `pivot`:** normalized `(0–1)`; `(0,0)` = top-left of parent/element (`Widget.hpp`).
- **`ContainsPoint`:** `point.y >= top && point.y < top + height` — Y-down hit testing.
- **Cursor (`GUIManager`):** `(0,0)` top-left; `camera_up` **decreases** Y (correct for this space).

**Source:** `ASCIICraft/include/ASCIICraft/gui/Widget.hpp`, `ASCIICraft/src/gui/GUIManager.cpp`

### 2.4 GUI quad mesh local space (`QuadMeshBuilder` + `GUIRenderer`)

- Mesh positions live on **`[-1, +1]²`** (unit square centered at origin).
- **`GUIRenderer`:** translates to rect center, scales by `halfSize`; `topLeftPx` is the **top-left** of the drawn rect.
- **Model +Y** = top of on-screen quad; **model −Y** = bottom (because `Camera2D` flips Y vs standard math Y-up).

**Source:** `ASCIICraft/src/util/QuadMeshBuilder.cpp`, `ASCIICraft/src/gui/GUIRenderer.cpp`

---

## 3. Texture coordinates

### 3.1 Engine upload path (authoritative for GPU)

| Step | Convention |
|------|------------|
| **stb load** | `stbi_set_flip_vertically_on_load(0)` — row 0 = **top** of image |
| **CPU `GetPixel(x,y)`** | `y=0` = top row |
| **D3D11 `UpdateSubresource`** | First row → top of GPU texture |
| **HLSL `Sample()`** | **(0,0) = top-left**, V increases **downward** |

**Source:** `ASCIIgL/src/engine/Texture.cpp`, `ASCIIgL/src/renderer/resources/Renderer_Textures.cpp`

### 3.2 GUI atlases (`PixelRectToUV` + `QuadMeshBuilder`)

- **Atlas pixel rects:** Minecraft-style — `(x, y)` = **top-left** of region in PNG space.
- **`PixelRectToUV`:** maps to D3D UVs; returns `(minU, bottomV, maxU, topV)` for the quad vertex layout (bottom vert uses `.y`, top vert uses `.w`).
- **Full quad UVs:** screen-top → `v=0`, screen-bottom → `v=1`.

**Source:** `ASCIICraft/include/ASCIICraft/util/QuadMeshBuilder.hpp`, `ASCIICraft/src/util/QuadMeshBuilder.cpp`

### 3.3 Terrain / block textures (different path)

Block mesh builders use a **separate intermediate UV space**, then flip at emit time:

```cpp
v.SetUV(glm::vec2(faceUVs[i].x, 1.0f - faceUVs[i].y)); // match existing V flip convention
```

Same pattern in `CubeModelBuilder`, `WaterModelBuilder`, `BlockModelBaker`.

- **JSON / default face UVs:** built in **0–1 block-atlas space** with Minecraft-style formulas (`BuildDefaultFaceUVs` uses `1.0f - y` for side faces).
- **At vertex emit:** **`v_gpu = 1.0f - v_internal`** to match D3D.
- **Do not** apply `PixelRectToUV` logic to terrain; terrain never goes through that helper.

**Source:** `ASCIICraft/src/world/block/models/BlockModelBaker.cpp`

### 3.4 Unit-quad vs pixel-space UV assignment

Two valid D3D layouts exist depending on how quad **positions** are authored:

| Pattern | Position space | Bottom vertex V | Top vertex V | Used by |
|---------|----------------|-----------------|--------------|---------|
| **Unit quad** | Model `[-1,+1]²`, +Y = screen-top via `Camera2D` | `1.0` | `0.0` | `QuadMeshBuilder`, `ItemIndex::GetQuadItemMesh` |
| **Pixel quad** | Direct `Camera2D` pixels, +Y = down | `1.0` at larger Y | `0.0` at smaller Y | `TextLabelMeshBuilder` |

Both map screen-top → texture `v=0` and screen-bottom → `v=1`. Do not copy unit-quad UV tuples onto pixel-space verts (or vice versa) without swapping V.

**Source:** `ASCIICraft/src/util/QuadMeshBuilder.cpp`, `ASCIICraft/src/ecs/data/ItemIndex.cpp`, `ASCIICraft/src/gui/text/TextLabelMeshBuilder.cpp`

---

## 4. 3D world space

### 4.1 Axes (consistent in rendering)

- **+Y = up** (gravity negative Y, terrain height increases with Y).
- **Block meshes** placed at integer `WorldCoord` with offset in chunk mesh gen (`worldOffset` added to vertices).
- **Block model space:** JSON coords `0–16` per axis → multiplied by `1/16` for `[0,1]` block space.

### 4.2 Face indices (`FaceDir` ↔ culling ↔ geometry)

Internal rendering uses one consistent 6-face index order (`FaceCulling.cpp`):

| Face index | `FaceDir` | Neighbor offset | Outward normal |
|------------|-----------|-----------------|----------------|
| 0 | Top | `(0,+1,0)` | +Y |
| 1 | Bottom | `(0,−1,0)` | −Y |
| 2 | North | `(0,0,+1)` | **+Z** |
| 3 | South | `(0,0,−1)` | **−Z** |
| 4 | East | `(+1,0,0)` | +X |
| 5 | West | `(−1,0,0)` | −X |

`BlockModelBaker` JSON face names (`north` → +Z face, etc.) match this **internal** enum.

**Source:** `ASCIICraft/include/ASCIICraft/world/block/state/FaceDir.hpp`, `ASCIICraft/src/world/block/FaceCulling.cpp`, `ASCIICraft/src/world/block/models/BlockModelBaker.cpp`

### 4.3 Blockstate cardinals vs mesh `FaceDir`

Two related types serve different jobs:

| Type | Purpose | North offset | South offset |
|------|---------|--------------|--------------|
| **`BlockCardinal`** | Blockstate properties, fence connections, gameplay neighbors | **−Z** | **+Z** |
| **`FaceDir`** | Mesh faces, culling, JSON face baking | **+Z face** (`FaceDirNeighborOffset`) | **−Z face** |

Use **`BlockCardinal`** + `NeighborCoord()` / `BlockCardinalOffset()` for anything driven by blockstate `north`/`south`/`east`/`west` (Minecraft / vanilla JSON).

Use **`FaceDir`** + `FaceDirNeighborOffset()` for rendering and face-index logic.

```cpp
// Blockstate: connect fence "north" toward -Z
NeighborCoord(position, BlockCardinal::North);

// Mesh: block across the +Z face
FaceDirNeighborOffset(FaceDir::North); // {0, 0, +1}
```

Fence placement and fence neighbor refresh both iterate `kHorizontalBlockCardinals`.

**Source:** `ASCIICraft/include/ASCIICraft/world/block/state/BlockCardinal.hpp`, `FaceDir.hpp`, `FencePlacement.cpp`, `BlockUpdateSystem.cpp`

### 4.4 Inconsistency: camera debug cardinals

```cpp
// yaw: east = -Z, west = +Z, north = +X, south = -X (matches your WorldCoord convention)
if (std::abs(front.x) > std::abs(front.z))
    return front.x > 0.0f ? "West" : "East";
else
    return front.z > 0.0f ? "South" : "North";
```

This debug mapping (**north = +X**, **east = −Z**, etc.) does **not** match `FaceDir`, fence neighbors, or standard Minecraft cardinals. Treat it as debug-only, not as world truth.

**Source:** `ASCIICraft/src/ecs/systems/CameraSystem.cpp`

### 4.5 Vanilla Minecraft JSON note

Vanilla block models put the **`north` face at min Z (z=0)**. This engine’s baker places **`north` at max Z (z2)**. Assets may still look correct if they were authored or converted for this mapping, but **do not assume vanilla `(x,y)` face semantics match 1:1**.

**Source:** `ASCIICraft/src/world/block/models/BlockModelBaker.cpp` (`BuildElementFaceVertsModelSpace`)

---

## 5. GPU pipeline spaces

### 5.1 Main 3D pass

- **Viewport:** `(0,0)` top-left, size = `Screen` cell dimensions.
- **NDC:** GLM `perspective` + `lookAt` (right-handed, Y-up world). No `GLM_FORCE_DEPTH_ZERO_TO_ONE` in CMake — clip-space Z follows GLM defaults, while D3D11 raster expects `[0,1]` after viewport transform (worth knowing if you debug depth issues).
- **Depth clear:** `1.0` = far (`Renderer_Draw.cpp`).

**Source:** `ASCIIgL/src/renderer/core/Renderer_Draw.cpp`, `ASCIIgL/CMakeLists.txt`

### 5.2 Quantization fullscreen pass

```hlsl
o.pos = float4(v.pos.x, -v.pos.y, 0.0, 1.0);
o.uv = (v.pos + 1.0) * 0.5;
```

- Flips Y when building clip position so **UV (0,0) samples the top-left** of the resolved color texture.
- **`SV_Position.xy`** in the PS = **pixel coords**, origin **top-left**, +Y down — aligned with screen/GUI.

**Source:** `ASCIIgL/src/renderer/device/Renderer_Device.cpp` (`QUANTIZATION_VS_SRC`)

### 5.3 ASCII window pass (`--window`)

- Uses `uint2(svPos.xy)` as pixel index; `cell = pix / cellPixels` — same top-left origin.
- Font atlas UV: `(inCell + 0.5) / cellPixels` — **(0,0) at top-left of glyph cell** in atlas slice.

**Source:** `ASCIIgL/src/renderer/presentation/Renderer_AsciiWindow.cpp`

### 5.4 Readback → terminal

- `DownloadFramebuffer` copies row `y=0..h-1` linearly into `pixelBuffer` — **no Y flip**; matches terminal top-left indexing.

**Source:** `ASCIIgL/src/renderer/device/Renderer_Device.cpp` (`DownloadFramebuffer`)

---

## 6. Chunk / storage indexing

```cpp
inline int GetBlockIndex(int x, int y, int z) {
    return x + y * sizes::CHUNK_SIZE + z * sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;
}
```

- Local chunk coords: `(0,0,0)` at chunk minimum corner; **Y is the middle stride** (X fastest, then Y, then Z).

**Source:** `ASCIICraft/include/ASCIICraft/world/chunk/ChunkUtil.hpp`

---

## 7. Legacy / unused paths

- **`PosWUVInvW`:** documented for an old CPU renderer (`VertFormat.hpp`); not part of the current D3D GUI/terrain path.
- **`ModelBuilderUtil::GetFaceUVs()`:** corner order `(0,0),(1,0),(1,1),(0,1)` in abstract UV space — meaning depends on the **`1-v` flip** at emit.

---

## 8. Summary of real inconsistencies

| Area | Issue |
|------|--------|
| **Horizontal cardinals** | Use `BlockCardinal` for blockstate/gameplay (−Z north); use `FaceDir` only for mesh/culling (+Z = `FaceDir::North` face) |
| **Texture V on 2D quads** | GUI `QuadMeshBuilder` + `PixelRectToUV` → D3D; terrain → internal UV + `1-v`; use unit-quad vs pixel-quad table in §3.4 |
| **Screen units** | “Pixels” in GUI often mean **character cells**; window mode multiplies by `cellPixels` for HWND size |
| **Minecraft JSON Z** | Baker `north`/`south` faces sit on ±Z opposite to vanilla model files (may be intentional for your assets) |

---

## 9. Practical rules when adding features

1. **GUI layout, hit tests, atlases:** top-left origin, Y down, D3D UVs (`PixelRectToUV` + `QuadMeshBuilder`).
2. **Terrain / block UVs:** use block baker paths; apply **`1.0f - v`** at vertex emit unless you’re explicitly matching an existing builder.
3. **World neighbors / gameplay:** use **`BlockCardinal`** and `NeighborCoord()` for blockstate north/south/east/west; use **`FaceDir`** only for mesh/culling.
4. **Screen sizing:** always clarify whether a value is **cells** (`Screen::GetWidth`) or **window pixels** (`cells × cellPixels`).
5. **New textured 2D quads:** use **`QuadMeshBuilder`** for `[-1,+1]` unit quads, or follow **`TextLabelMeshBuilder`** for direct pixel verts — see §3.4.

---

## 10. Key source files

| Topic | Path |
|-------|------|
| 2D camera | `ASCIIgL/src/engine/Camera2D.cpp` |
| 3D camera | `ASCIIgL/src/engine/Camera3D.cpp` |
| Screen / terminal buffer | `ASCIIgL/src/renderer/screen/ScreenTerminalImpl.cpp` |
| GUI widgets | `ASCIICraft/include/ASCIICraft/gui/Widget.hpp` |
| GUI quads / atlas UVs | `ASCIICraft/src/util/QuadMeshBuilder.cpp` |
| Block face UVs | `ASCIICraft/src/world/block/models/BlockModelBaker.cpp` |
| Face culling indices | `ASCIICraft/src/world/block/FaceCulling.cpp` |
| Quantization pass | `ASCIIgL/src/renderer/device/Renderer_Device.cpp` |
| Blockstate cardinals | `ASCIICraft/include/ASCIICraft/world/block/state/BlockCardinal.hpp` |
| Mesh face dirs | `ASCIICraft/include/ASCIICraft/world/block/state/FaceDir.hpp` |
| World coords | `ASCIICraft/include/ASCIICraft/world/Coords.hpp` |
