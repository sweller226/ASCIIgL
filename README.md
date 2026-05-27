# ASCIIgL / ASCIICraft Developer Setup

This guide is for contributors who want to clone, build, run, and profile the project with minimal trial-and-error.

## Repository Layout

- `ASCIIgL/` engine source
- `ASCIICraft/` game source
- `scripts/` top-level helper scripts
- `tools/` local tooling (for example Tracy viewer binaries)

## Prerequisites

- Windows 10/11
- Visual Studio with C++ desktop workload (generator used by scripts: `Visual Studio 18 2026`)
- CMake 3.16+
- PowerShell
- Git with submodule support

## External Dependencies and Versions

- Engine vendor dependencies are Git submodules under `ASCIIgL/vendor/`:
  - `glm`
  - `nlohmann/json`
  - `stb`
  - `tinyobjloader`
  - `tracy`
- oneTBB is required by ASCIICraft and is not committed to git.
  - Expected folder: `ASCIICraft/lib/oneTBB/oneapi-tbb-2022.3.0-win/oneapi-tbb-2022.3.0`
  - Expected runtime libs include `tbb12.dll` and `tbb12_debug.dll`
- Tracy viewer tools are local binaries (not built by default from source in this repo).
  - Suggested location: `tools/Tracy/`
  - Current local bundle in this repo is under `tools/Tracy/windows-0.13.1`

## First-Time Clone Setup

1. Clone with submodules:
   - `git clone --recurse-submodules <repo-url>`
2. If already cloned without submodules:
   - `git submodule update --init --recursive`
3. Verify submodules:
   - `git submodule status`
4. Place oneTBB at the expected path:
   - `ASCIICraft/lib/oneTBB/oneapi-tbb-2022.3.0-win/oneapi-tbb-2022.3.0`
5. Put Tracy executables under `tools/Tracy/` (already scripted via `scripts/run_tracy.ps1`).

## Build Commands

### Build Engine (ASCIIgL)

From `ASCIIgL/`:

- FastDebug:
  - `./scripts/build_debug.ps1`
- Release:
  - `./scripts/build_release.ps1`

Note: `build_debug.ps1` currently builds the `FastDebug` configuration.

### Build Game (ASCIICraft)

From `ASCIICraft/`:

- FastDebug:
  - `./scripts/build_fastdebug.ps1`
- Release:
  - `./scripts/build_release.ps1`

### Build + Package + Deploy Engine to ASCIICraft

From repo root:

- `./scripts/build_ASCIIgL_ASCIICraft.ps1`

This script builds engine artifacts, creates a distribution, and deploys it into `ASCIICraft/lib/ASCIIgL-v1.0.0`.

## Run Commands

### ASCIICraft FastDebug

- `cd ASCIICraft/build/bin/FastDebug`
- `./ASCIICraft.exe`
- Window mode: `./ASCIICraft.exe --window`

### ASCIICraft Release

- `cd ASCIICraft/build/bin/Release`
- `./ASCIICraft.exe`
- Window mode: `./ASCIICraft.exe --window`

## Tracy Profiling

- Instrumentation is enabled in CMake with `TRACY_ENABLE`.
- Run Tracy tools via helper script from repo root:
  - Viewer: `./scripts/run_tracy.ps1`
  - Capture: `./scripts/run_tracy.ps1 capture -- -h`
  - CSV export: `./scripts/run_tracy.ps1 csvexport -- -h`
- Typical flow:
  1. Start `ASCIICraft` (FastDebug preferred for iteration)
  2. Launch Tracy viewer
  3. Connect to process and capture

## Terminal/Font Notes (ASCII output quality)

If terminal rendering does not look correct:

- Use Square Modern font from engine resources
- Set terminal palette expected by the program
- Set line height to `1.0`
- Windows Terminal antialiasing:
  - `cleartype` for readability
  - `aliased` for stricter ASCII art look

## Common Setup Failures

- Submodule headers missing:
  - Run `git submodule update --init --recursive`
- CMake cannot find oneTBB:
  - Check exact oneTBB path under `ASCIICraft/lib/oneTBB/...`
- Missing Tracy viewer executable:
  - Place Tracy binaries under `tools/Tracy/` and use `scripts/run_tracy.ps1`
- Generator error for Visual Studio 18:
  - Install matching VS toolchain or adjust script generator to your installed version

## Quick Checklist

- Submodules initialized
- oneTBB folder present at expected path
- Tracy binaries present under `tools/Tracy`
- `ASCIIgL` builds FastDebug/Release
- `ASCIICraft` builds FastDebug/Release
