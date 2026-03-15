# Font Atlas vs coverage_cleartype.json

## Why the atlas looks the same for every font size

- The font atlas is built **once** in `Renderer::InitializeFontAtlas()` during startup.
- It uses `Screen::GetInst().GetFontSize()` and `CoverageJson::GetCellSizeForFontSize(fontSize)` at that moment.
- If you change font size later (e.g. settings or a different run), the atlas is **not** rebuilt, so it always shows glyphs for the font size at init.

**Fix:** Rebuild the font atlas when font size (or cell size) changes, or ensure the app always uses the same font size at first frame so the single build is correct.

---

## How coverage_cleartype.json is produced (CharCoverage tool)

From `tools/CharCoverage/char_coverage.cpp`:

| Parameter        | CharCoverage (JSON) | Our FontAtlasBuilder (current) |
|------------------|---------------------|---------------------------------|
| **pixelsPerDip** | `1.0f`              | `2.0f` (supersample)           |
| **Cell size**    | `computeCellSizePixels(pointSize, 1.0f)` → round(pointSize×96/72) | From JSON `GetCellSizeForFontSize(fontSize)` |
| **Raster**       | 1:1 (bounds = glyph at 1 DIP) | 2× then 2×2 downsample         |
| **ClearType**    | `--cleartype` → CLEARTYPE_NATURAL, 3×1 texture | Same (USE_CLEARTYPE true)       |
| **Font**         | Square Modern / Square | Same (GetFontFaceFromSystem)   |

So the **coverage** values in the JSON are computed from glyphs rasterized at **1 pixel per DIP**. Our atlas is currently rasterized at **2 pixels per DIP** and then downsampled, so the effective shape can differ slightly from what the tool “sees,” and the pipeline is not the same.

---

## Aligning the atlas with the coverage JSON

To have the font atlas drawn **the same way** as in the tool that generates `coverage_cleartype.json`:

1. **Use `pixelsPerDip = 1.0f`** in the atlas builder (same as CharCoverage).
2. **Use the same cell size source**: keep using `GetCellSizeForFontSize(fontSize)` so cell dimensions match the JSON intervals.
3. **Do not supersample**: rasterize at 1:1 and write directly into the cell (no 2× raster + 2×2 box filter). Then the atlas bitmap is exactly the same raster the coverage tool uses for that font size and cell.
4. **Same DirectWrite pipeline**: same font, same `CreateGlyphRunAnalysis` (ClearType natural), same `DWRITE_TEXTURE_CLEARTYPE_3x1`, same (R,G,B) → atlas and subpixel in shader.

**Trade-off:** At 1 DIP, small point sizes can give very low-resolution glyphs (e.g. 3×3) and may look binary (0/255 alpha). That matches what the coverage tool uses; if you want smoother edges at runtime you could keep an optional 2× path for display only, while still using 1 DIP for “coverage-accurate” atlas if needed.

---

## Recommendation

- **Default:** Build the atlas with the **same** pipeline as the CharCoverage tool (`pixelsPerDip = 1.0f`, 1:1 write, cell size from JSON). Then the atlas is consistent with `coverage_cleartype.json` and will **change with font size** when you rebuild the atlas (e.g. when font size changes).
- **Rebuild on font size change:** When `Screen` font size or the chosen coverage interval changes, call a “recreate font atlas” path so the atlas is rebuilt with the new size and cell dimensions.

Implementing the “match coverage” pipeline in the builder and adding a note about rebuilding the atlas when font size changes will give you something concrete to review and test.
