#include <ASCIIgL/util/CoverageJson.hpp>

#include <fstream>
#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace ASCIIgL {

static const wchar_t* DEFAULT_CHAR_RAMP = L" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890.:,;'\"(!?)+-*/=\"";

const wchar_t* CoverageJson::GetDefaultCharRamp() {
    return DEFAULT_CHAR_RAMP;
}

namespace {

static nlohmann::json s_json;
static bool s_loaded = false;

static const char* DEFAULT_PATHS[] = {
    "res/ASCIIgL/coverage_cleartype.json",
};

/// Compute cell size in pixels from point size (same formula as CharCoverage tool).
static void computeCellSizeFromPointSize(float pointSize, int* outX, int* outY) {
    float fontEmSizeDIP = pointSize * 96.0f / 72.0f;
    float cellPx = fontEmSizeDIP * 1.0f;
    if (cellPx < 1.0f) cellPx = 1.0f;
    int cell = static_cast<int>(cellPx + 0.5f);
    if (cell < 1) cell = 1;
    *outX = cell;
    *outY = cell;
}

} // namespace

bool CoverageJson::Load() {
    if (s_loaded) return !s_json.empty();
    for (const char* path : DEFAULT_PATHS) {
        std::ifstream file(path);
        if (!file.good()) continue;
        try {
            s_json = nlohmann::json::parse(file);
            s_loaded = true;
            return true;
        } catch (const std::exception&) {
            continue;
        }
    }
    s_loaded = true; // don't retry
    return false;
}

bool CoverageJson::LoadFromPath(const char* path) {
    std::ifstream file(path);
    if (!file.good()) return false;
    try {
        s_json = nlohmann::json::parse(file);
        s_loaded = true;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool CoverageJson::GetIntervalForFontSize(float fontSize, CoverageInterval& out) {
    out.coverages.clear();
    out.chars.clear();
    out.cellPixelsX = 0;
    out.cellPixelsY = 0;
    out.sizeMin = 0.f;
    out.sizeMax = 0.f;

    if (!Load() || !s_json.contains("intervals") || !s_json["intervals"].is_array() || s_json["intervals"].empty())
        return false;

    const auto& intervals = s_json["intervals"];
    size_t bestIdx = 0;
    bool found = false;
    for (size_t i = 0; i < intervals.size(); ++i) {
        const auto& iv = intervals[i];
        if (!iv.contains("sizeMin") || !iv.contains("sizeMax") || !iv.contains("coverages")) continue;
        float smin = iv["sizeMin"].get<float>();
        float smax = iv["sizeMax"].get<float>();
        if (fontSize >= smin && fontSize <= smax) {
            bestIdx = i;
            found = true;
            break;
        }
    }
    if (!found && !intervals.empty()) {
        if (fontSize <= intervals[0]["sizeMin"].get<float>())
            bestIdx = 0;
        else
            bestIdx = intervals.size() - 1;
    }

    const auto& iv = intervals[bestIdx];
    const auto& coverages = iv["coverages"];
    if (!coverages.is_array()) return false;

    out.sizeMin = iv["sizeMin"].get<float>();
    out.sizeMax = iv["sizeMax"].get<float>();

    const size_t jsonN = coverages.size();
    out.coverages.reserve(jsonN);
    for (size_t i = 0; i < jsonN; ++i)
        out.coverages.push_back(coverages[i].get<float>());

    const bool hasChars = s_json.contains("chars") && s_json["chars"].is_array() && s_json["chars"].size() >= jsonN;
    out.chars.reserve(jsonN);
    for (size_t i = 0; i < jsonN; ++i) {
        unsigned cp = hasChars ? s_json["chars"][i].get<unsigned>() : static_cast<unsigned>(DEFAULT_CHAR_RAMP[i]);
        out.chars.push_back(cp);
    }

    if (iv.contains("cellPixelsX") && iv.contains("cellPixelsY")) {
        out.cellPixelsX = iv["cellPixelsX"].get<int>();
        out.cellPixelsY = iv["cellPixelsY"].get<int>();
    } else {
        float mid = (out.sizeMin + out.sizeMax) * 0.5f;
        computeCellSizeFromPointSize(mid, &out.cellPixelsX, &out.cellPixelsY);
    }
    return true;
}

bool CoverageJson::GetCellSizeForFontSize(float fontSize, int* outCellPixelsX, int* outCellPixelsY) {
    if (!outCellPixelsX || !outCellPixelsY) return false;
    CoverageInterval interval;
    if (!GetIntervalForFontSize(fontSize, interval)) return false;
    *outCellPixelsX = interval.cellPixelsX;
    *outCellPixelsY = interval.cellPixelsY;
    return true;
}

} // namespace ASCIIgL
