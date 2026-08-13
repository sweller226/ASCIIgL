# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository layout

This repo holds two CMake projects plus shared tooling:

- `ASCIIgL/` — the rendering engine, built as a static library (`ASCIIgL::ASCIIgL`). Public API lives under `ASCIIgL/include/ASCIIgL/`; implementation under `ASCIIgL/src/`.
- `ASCIICraft/` — a voxel game built on the engine, built as an executable. Public-ish headers under `ASCIICraft/include/ASCIICraft/`; implementation under `ASCIICraft/src/` (mirrors the include tree).
- `scripts/` — top-level orchestration scripts (build+deploy both projects, Tracy helper, char-coverage regen).
- `tools/CharCoverage/` — standalone CMake tool that scans installed fonts/terminal glyph coverage and emits the JSON the renderer uses for ASCII quantization.
- `tools/Tracy/` — local Tracy profiler viewer binaries (not built from source here).

`ASCIICraft` does **not** consume `ASCIIgL` as a sibling source tree at build time — it depends on a packaged distribution at `ASCIICraft/lib/ASCIIgL-v1.0.0` produced by `create_distribution.ps1`/the combined build script. If you change engine code, you must rebuild+redeploy before ASCIICraft picks it up (see Commands below). Some vendor deps are shared by reaching across the sibling tree instead of duplicating: ASCIICraft's CMakeLists pulls `nlohmann_json` headers and the `oneTBB` submodule directly from `ASCIIgL/vendor/`.

Both projects only define `FastDebug` and `Release` configurations (no plain `Debug`) and target C++17 with AVX2/FMA and fast-math enabled unconditionally in the compiler flags — expect reduced strict floating-point determinism.

## Commands

Build scripts are PowerShell and must be run from the directory noted (or via the wrapper at repo root).

```
# Engine only, from ASCIIgL/
./scripts/build_debug.ps1      # FastDebug config
./scripts/build_release.ps1

# Game only, from ASCIICraft/
./scripts/build_fastdebug.ps1
./scripts/build_release.ps1

# Engine -> distribution -> deploy into ASCIICraft/lib/, from repo root
./scripts/build_ASCIIgL_ASCIICraft.ps1
```

Run the game from its build output directory (resources are copied alongside the exe):

```
cd ASCIICraft/build/bin/FastDebug   # or Release
./ASCIICraft.exe                    # Windows Terminal output, full 16-color palette
./ASCIICraft.exe --window           # native window instead of console
./ASCIICraft.exe --mono             # monochrome quantization
```

There is no test suite in this repo (no CTest targets, no test source directories) — verification is manual, by running the game.

Profiling (Tracy instrumentation is compiled in via `TRACY_ENABLE`):

```
./scripts/run_tracy.ps1                 # viewer
./scripts/run_tracy.ps1 capture -- -h
./scripts/run_tracy.ps1 csvexport -- -h
```

Regenerating the character-coverage LUT used for ASCII quantization (only needed after changing font/coverage logic):

```
cd tools/CharCoverage && ./build.ps1
./scripts/new_coverage_ASCIIgL.ps1 -Rebuild -Regenerate
```

`build_commands.txt` at repo root has the full set of ad hoc commands (including per-tool build/run variants) if a script above doesn't cover what you need.

## Architecture

### Rendering pipeline (ASCIIgL)

The core idea: render a normal 3D frame with DirectX 11, then quantize it to characters instead of displaying it as pixels.

1. `Renderer` (singleton, `ASCIIgL/include/ASCIIgL/renderer/Renderer.hpp`, impl split across `src/renderer/core/`, `src/renderer/device/`, `src/renderer/resources/`, `src/renderer/presentation/`) draws the scene into an off-screen RGB target via a queued draw-call list (`BeginGpuFrame` → `SubmitDraw` → `FlushDraws` → `EndGpuFrame`), sorted opaque-then-transparent.
2. On startup, block/item textures are clustered into a 16-color `Palette` in Oklab space (`renderer/Palette.*`, `renderer/PaletteUtil.*`).
3. A precomputed lookup table (`PrecomputeColorLUT` / `PrecomputeMonochromeColorLUT` / `PrecomputeMultiColorLUT` in `Renderer`) maps quantized colors to the best foreground/background glyph pair, scored against character coverage data loaded from JSON (`util/CoverageJson.*`, produced by `tools/CharCoverage`).
4. The quantized frame is presented either to a Windows Terminal console (`renderer/screen/ScreenTerminalImpl`) or a native window (`renderer/screen/ScreenWindowImpl`) — selected via `Screen`/`ScreenImpl`.

`Renderer::Impl` (defined in `src/renderer/core/RendererImpl.hpp`) holds all D3D11 state; the public header exposes only the draw-call API and settings toggles. When adding renderer functionality, prefer extending the relevant `Renderer_*.cpp` (Draw/States/Device/Textures/AsciiWindow) rather than growing one file — this split-by-concern-with-underscore-suffix pattern is used throughout the engine and mirrors into ASCIICraft (e.g. `TerrainGenerator_*.cpp`).

### Game (ASCIICraft)

- Entity/component model uses `entt::registry`. Components live under `ecs/components/`, systems under `ecs/systems/` implementing `ecs::systems::ISystem::Update()`. `Game` (`game/Game.hpp`/`.cpp`) owns the registry, the `ASCIIgL::EventBus`, and every system as a plain member — system execution order in `Game::Update`/`Game::Render` (and member declaration order) *is* the frame's update order; there's no scheduler. Cross-system communication goes through `EventBus` (per-frame typed event buffers, cleared each frame via `endFrame()`), not direct references between systems, for things like block break/place, GUI, sound, and particle spawn events (`events/`).
- World data is chunked (`world/chunk/`): `ChunkManager` owns loaded chunks and drives streaming based on player position, offloading terrain generation and mesh building to worker threads through `ChunkJobQueue` (built on the shared `oneTBB` submodule) and draining completed results on the main thread each frame (`DrainAndApplyJobResults`). Block placement/mining edits that cross chunk boundaries are buffered in `crossChunkEdits` until the target chunk loads.
- Block definitions are data-driven JSON (`world/block/state/JsonBlockStateLoader`, `world/block/models/JsonModelLoader`) resolved into runtime `BlockState`/`ResolvedBlockModel` objects at load time, not hardcoded per-block classes.
- GUI (`gui/`) is a small retained-mode widget system (`Panel`, `Slot`, `Widget`) driven by `GuiManager`, with screens (`InventoryScreen`, `PlayHUDScreen`) as the composition points.

### Threading

Both the engine's LUT precompute and the game's chunk terrain/meshing use `oneTBB`, shared as a single submodule under `ASCIIgL/vendor/oneTBB` (ASCIICraft's CMake reaches into the sibling tree for it rather than vendoring its own copy). Worker-thread work is always drained/applied on the main thread once per frame — avoid introducing cross-thread mutation of ECS/registry state outside that drain step.
