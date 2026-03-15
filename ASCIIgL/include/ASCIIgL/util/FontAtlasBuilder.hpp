#pragma once

#include <vector>
#include <cstdint>

namespace ASCIIgL {

/// Result of building a font atlas: one RGBA8 image per glyph (slice).
/// ClearType: R,G,B = per-channel coverage (for subpixel rendering); A=255.
/// Non-ClearType: R=G=B=gray alpha, A=255.
struct FontAtlasBuildResult {
    bool success = false;
    int cellPixelsX = 0;
    int cellPixelsY = 0;
    int glyphCount = 0;
    /// One RGBA8 image per glyph; each of size cellPixelsX * cellPixelsY * 4 (row-major).
    std::vector<std::vector<std::uint8_t>> slices;
};

/// Build font atlas using DirectWrite. One slice per character in charRamp (order preserved).
/// \param fontPath  Path to .ttf file (e.g. res/ASCIIgL/fonts/square/square.ttf). If null or empty, uses system font "Square Modern" / "Square".
/// \param pointSize Font size in points (same as Screen/CharCoverage).
/// \param charRamp  Null-terminated wide string of characters (e.g. CoverageJson::GetDefaultCharRamp()).
/// \param cellPixelsX  Cell width in pixels (e.g. from CoverageJson::GetCellSizeForFontSize).
/// \param cellPixelsY  Cell height in pixels.
/// \return Build result with slices for Texture2DArray upload; success false on failure.
FontAtlasBuildResult BuildFontAtlasFromDirectWrite(
    const wchar_t* fontPath,
    float pointSize,
    const wchar_t* charRamp,
    int cellPixelsX,
    int cellPixelsY
);

} // namespace ASCIIgL
