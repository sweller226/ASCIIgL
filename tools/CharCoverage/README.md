# Char Coverage Tool

Analytically computes **character coverage** values for the ASCII art luminance ramp used by the renderer. These values are used in `Renderer::PrecomputeColorLUT()` to pick the best character for a given luminance: `simLuminance = coverage * fgLuminance + (1 - coverage) * bgLuminance`.

## What "coverage" means

For a character rendered in a cell (e.g. in Windows Terminal):

- **Coverage** = fraction of the cell that appears as **foreground** (the glyph ink).
- So `0.0` = empty (space), `~0.67` = very dense (e.g. @).

With anti-aliasing, coverage is the **mean alpha** of the cell when the glyph is rendered white-on-black (or black-on-white, then use 1 − mean). This tool uses DirectWrite to render each ramp character at your font and size and computes that mean.

## Why run this

The ramp in `Renderer.hpp` (`_charRamp` / `_charCoverage`) was previously eyeballed. Running this tool:

- Uses the **same rendering pipeline** as Windows Terminal (DirectWrite).
- Produces **per–font-size** values (coverage changes with size and font).
- Lets you **regenerate** values when you change font or point size.

## Usage

### Build (Windows, Visual Studio or cl.exe)

From the repo root:

```powershell
# Using Developer Command Prompt or VS environment
cl.exe tools\CharCoverage\char_coverage.cpp /Fe:tools\CharCoverage\char_coverage.exe /link dwrite.lib
```

Or open a CMake project in `tools/CharCoverage` and build (see `CMakeLists.txt`).

### Run

```text
char_coverage.exe [options]
```

**Options:**

| Option | Description | Default |
|--------|-------------|--------|
| `--font "Name"` | Font family (as in WT settings) | `"Square Modern"` |
| `--size <float>` | Point size (as in WT) | `3.0` |
| `--chars " .-:o+OAE0B@"` | Characters to measure | Ramp from Renderer.hpp |
| `--cleartype` | Use ClearType antialiasing (matches Windows Terminal); default is aliased 1x1 | Off |
| `--scan` | Run for every size 2.0–11.0 pt step 0.01; output **one** unique coverage set per interval (consecutive sizes with identical coverages are merged) | Off |
| `--code` | Emit C++ array snippet | Off (print table only) |
| `--json` | Emit JSON | Off |

**Examples:**

```powershell
# Default: Square Modern @ 3pt, print table
.\char_coverage.exe

# Same, emit C++ for pasting into Renderer.hpp
.\char_coverage.exe --code

# Scan 2–11 pt step 0.01, output unique coverages per interval (one set per range where coverages don't change)
.\char_coverage.exe --scan

# ClearType (matches Windows Terminal antialiasing)
.\char_coverage.exe --cleartype --size 3 --code
.\char_coverage.exe --cleartype --scan -o coverage_cleartype.txt

# Try another size (single size)
.\char_coverage.exe --size 10 --code

# Custom font
.\char_coverage.exe --font "Consolas" --size 12 --code
```

### Output

- **Scan mode** (`--scan`): outputs **JSON** with `font`, `sizeMin`, `sizeMax`, `step`, `cleartype`, and `intervals` (array of `{ "sizeMin", "sizeMax", "coverages": [...] }`). Use `-o coverage.json` to save for loading in another program.
- **Single size (table):** one line per character with `coverage` in [0, 1].
- **--code:** `static constexpr std::array<float, N> _charCoverage = { ... };` with a short comment per value.
- **--json** (single size): `{ "font": "...", "size": ..., "cleartype": ..., "coverages": [ ... ] }` for automation.

## How it works

1. **DirectWrite** loads the font (system or custom) at the given point size (converted to DIPs).
2. For each character, a **glyph run** (single glyph) is analyzed with `CreateGlyphRunAnalysis` (aliased or ClearType mode per `--cleartype`).
3. **GetAlphaTextureBounds** + **CreateAlphaTexture** yield the alpha bitmap: **aliased** = 1 byte per pixel; **ClearType** = 3 bytes per pixel (R,G,B subpixel).
4. **Coverage** = (sum of alpha values) / (255 × byte count). For ClearType this is the average of the three channels, matching the effective antialiased coverage Windows Terminal uses.

So the values match what DirectWrite (and thus Windows Terminal) actually render at that font and size.

## When to re-run

- You change the **font** or **point size** in Windows Terminal (e.g. in `Game.hpp` `FONT_SIZE` or WT settings).
- You change the **character ramp** (`_charRamp`); re-run with `--chars "..."` to get matching `_charCoverage`.
- You add a new ramp and want analytical coverage instead of eyeballing.

## Integrating into the build

You can add a target that runs the tool and (optionally) overwrites a generated header or prints to stdout for manual paste. The renderer continues to use the static arrays in `Renderer.hpp`; this tool is only for generating the float array when you change font/size/ramp.
