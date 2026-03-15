#pragma once

#include <vector>
#include <cstddef>

namespace ASCIIgL {

/// Interval data for a font size range from the coverage JSON (tools/CharCoverage output).
struct CoverageInterval {
    std::vector<float> coverages;   ///< Coverage value per character (index matches chars)
    std::vector<unsigned> chars;   ///< Codepoints from JSON "chars" or default ramp
    int cellPixelsX = 0;
    int cellPixelsY = 0;
    float sizeMin = 0.f;
    float sizeMax = 0.f;
};

/// Centralized access to the coverage JSON file (res/ASCIIgL/coverage_cleartype.json).
/// Load is lazy on first use. Paths tried: res/ASCIIgL/coverage_cleartype.json.
namespace CoverageJson {

/// Default character ramp; must match tools/CharCoverage DEFAULT_CHARS.
const wchar_t* GetDefaultCharRamp();

/// Load JSON from default paths. Returns true if loaded successfully.
/// Called automatically by GetIntervalForFontSize / GetCellSizeForFontSize if not yet loaded.
bool Load();

/// Optional: load from a specific path (e.g. for tests). Returns true on success.
bool LoadFromPath(const char* path);

/// Find the interval that contains fontSize and fill \a out. Returns true if found.
/// If JSON has no "cellPixelsX"/"cellPixelsY" in the interval, they are derived from size.
bool GetIntervalForFontSize(float fontSize, CoverageInterval& out);

/// Get cell size in pixels for the given font size. Returns true if coverage data is available.
/// Outputs are unchanged and return false if load failed or no interval.
bool GetCellSizeForFontSize(float fontSize, int* outCellPixelsX, int* outCellPixelsY);

} // namespace CoverageJson

} // namespace ASCIIgL
