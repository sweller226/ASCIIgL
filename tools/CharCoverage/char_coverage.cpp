/**
 * Char Coverage Tool
 *
 * Computes character coverage values for the ASCII art ramp using DirectWrite
 * (same pipeline as Windows Terminal). Build with:
 *   cl.exe char_coverage.cpp /link dwrite.lib
 * Or use CMakeLists.txt in this directory.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

#pragma comment(lib, "dwrite.lib")

// Must match Windows Terminal font face (ScreenWinImpl: "Square Modern"), square cell, lineHeight 1
static const wchar_t* DEFAULT_FONT = L"Square Modern";
static const float DEFAULT_SIZE = 3.0f;
static const wchar_t DEFAULT_CHARS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890.:,;'\"(!?)+-*/=\"";

// Scan mode: sizes 2 to 12, step 0.01
static const float SCAN_SIZE_MIN = 2.0f;
static const float SCAN_SIZE_MAX = 11.0f;
static const float SCAN_STEP = 0.01f;

// DirectWrite alpha texture dimension limit (avoids CreateAlphaTexture failure at large sizes)
static const int MAX_ALPHA_TEXTURE_DIM = 4096;

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --font \"Name\"   Font family (default: Square Modern)\n"
        "  --size <float>  Point size (default: 3.0)\n"
        "  --chars \"...\"   Characters to measure (default: ASCIIgL::Renderer::_charRamp)\n"
        "  --cleartype    Use ClearType antialiasing (matches Windows Terminal); default is aliased\n"
        "  --scan         Scan sizes 2.0–11.0 step 0.01, output unique coverages per interval\n"
        "  --output <path>  Write results to file (default: stdout)\n"
        "  --code         Emit C++ array for Renderer.hpp\n"
        "  --json         Emit JSON\n",
        prog);
}

static bool coverageEqual(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static float computeCoverage(
    IDWriteFactory* factory,
    IDWriteFontFace* fontFace,
    float fontEmSizeDIP,
    wchar_t ch,
    float pixelsPerDip,
    bool useClearType,
    HRESULT* outHr
) {
    UINT32 codePoint = (UINT32)(unsigned short)ch;
    UINT16 glyphIndex = 0;
    HRESULT hr = fontFace->GetGlyphIndices(&codePoint, 1, &glyphIndex);
    if (FAILED(hr)) {
        static bool logged = false;
        if (!logged) { fprintf(stderr, "Note: GetGlyphIndices failed 0x%08X\n", (unsigned)hr); logged = true; }
        if (outHr) *outHr = hr;
        return -1.f;
    }

    // Get design metrics so we can provide explicit advance (required by CreateGlyphRunAnalysis on some configs)
    DWRITE_FONT_METRICS fontMetrics = {};
    fontFace->GetMetrics(&fontMetrics);
    DWRITE_GLYPH_METRICS glyphMetrics = {};
    hr = fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics);
    if (FAILED(hr)) {
        static bool logged = false;
        if (!logged) { fprintf(stderr, "Note: GetDesignGlyphMetrics failed 0x%08X\n", (unsigned)hr); logged = true; }
        if (outHr) *outHr = hr;
        return -1.f;
    }
    float advance = (float)glyphMetrics.advanceWidth * fontEmSizeDIP / (float)fontMetrics.designUnitsPerEm;

    DWRITE_GLYPH_RUN run = {};
    run.fontFace = fontFace;
    run.fontEmSize = fontEmSizeDIP;
    run.glyphCount = 1;
    run.glyphIndices = &glyphIndex;
    run.glyphAdvances = &advance;
    run.glyphOffsets = nullptr;
    run.isSideways = FALSE;
    run.bidiLevel = 0;

    DWRITE_RENDERING_MODE renderingMode = useClearType ? DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL : DWRITE_RENDERING_MODE_ALIASED;
    IDWriteGlyphRunAnalysis* analysis = nullptr;
    hr = factory->CreateGlyphRunAnalysis(
        &run,
        pixelsPerDip,
        nullptr,
        renderingMode,
        DWRITE_MEASURING_MODE_NATURAL,
        0.0f, 0.0f,
        &analysis
    );
    if (FAILED(hr) || !analysis) {
        static bool loggedRun = false;
        if (!loggedRun) { fprintf(stderr, "Note: CreateGlyphRunAnalysis failed 0x%08X\n", (unsigned)hr); loggedRun = true; }
        if (outHr) *outHr = hr;
        return -1.f;
    }

    DWRITE_TEXTURE_TYPE textureType = useClearType ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
    RECT bounds = {};
    hr = analysis->GetAlphaTextureBounds(textureType, &bounds);
    if (FAILED(hr)) {
        analysis->Release();
        static bool loggedBounds = false;
        if (!loggedBounds) { fprintf(stderr, "Note: GetAlphaTextureBounds failed 0x%08X\n", (unsigned)hr); loggedBounds = true; }
        if (outHr) *outHr = hr;
        return -1.f;
    }

    int w = bounds.right - bounds.left;
    int h = bounds.bottom - bounds.top;
    if (w <= 0 || h <= 0) {
        analysis->Release();
        return 0.0f; // empty glyph (e.g. space)
    }
    if (w > MAX_ALPHA_TEXTURE_DIM || h > MAX_ALPHA_TEXTURE_DIM) {
        analysis->Release();
        static bool logged = false;
        if (!logged) {
            fprintf(stderr, "Note: glyph bounds %d x %d exceed max %d (increase MAX_ALPHA_TEXTURE_DIM in source)\n", w, h, MAX_ALPHA_TEXTURE_DIM);
            logged = true;
        }
        if (outHr) *outHr = (HRESULT)0x80070057; /* E_INVALIDARG - bounds too large */
        return -1.f;
    }

    // ClearType: 3 bytes per pixel (R,G,B subpixel); Aliased: 1 byte per pixel
    size_t bytesPerPixel = useClearType ? 3u : 1u;
    size_t bufferSize = (size_t)w * (size_t)h * bytesPerPixel;
    std::vector<BYTE> buffer(bufferSize);
    hr = analysis->CreateAlphaTexture(textureType, &bounds, buffer.data(), (UINT32)bufferSize);
    analysis->Release();
    if (FAILED(hr)) {
        static bool loggedCreate = false;
        if (!loggedCreate) {
            fprintf(stderr, "Note: CreateAlphaTexture failed 0x%08X for bounds %d x %d (buffer size %zu)\n", (unsigned)hr, w, h, bufferSize);
            loggedCreate = true;
        }
        if (outHr) *outHr = hr;
        return -1.f;
    }

    double sum = 0.0;
    for (size_t i = 0; i < bufferSize; ++i)
        sum += buffer[i];

    // Normalize by fixed square cell (line height 1, cell width 1) so coverage = fraction of cell filled.
    // With pixelsPerDip 1.0, texture bounds are in pixels where 1 px = 1 DIP; cell = fontEmSizeDIP x fontEmSizeDIP.
    float cellPx = fontEmSizeDIP * pixelsPerDip;
    if (cellPx < 1.0f) cellPx = 1.0f;
    double cellArea = (double)(cellPx * cellPx);
    double maxSumPerCell = 255.0 * (double)bytesPerPixel * cellArea;
    float coverage = (float)(sum / maxSumPerCell);
    if (coverage > 1.0f) coverage = 1.0f;
    if (coverage < 0.0f) coverage = 0.0f;
    return coverage;
}

static bool computeCoveragesForSize(
    IDWriteFactory* factory,
    IDWriteFontFace* fontFace,
    const std::wstring& chars,
    float pointSize,
    float pixelsPerDip,
    bool useClearType,
    std::vector<float>& out,
    size_t* outFailedCharIndex,
    HRESULT* outHr
) {
    // Point to DIP: 1 pt = 1/72 inch, 1 inch = 96 DIP (fixed), so fontEmSizeDIP = pointSize * 96/72.
    float fontEmSizeDIP = pointSize * 96.0f / 72.0f;
    out.clear();
    out.reserve(chars.size());
    for (size_t i = 0; i < chars.size(); ++i) {
        float c = computeCoverage(factory, fontFace, fontEmSizeDIP, chars[i], pixelsPerDip, useClearType, outHr);
        if (c < 0.f) {
            if (outFailedCharIndex) *outFailedCharIndex = i;
            return false;
        }
        out.push_back(c);
    }
    return true;
}

struct Interval {
    float sizeMin = 0.f, sizeMax = 0.f;
    std::vector<float> coverages;
};

int main(int argc, char** argv) {
    std::wstring fontName = DEFAULT_FONT;
    float pointSize = DEFAULT_SIZE;
    std::wstring chars = DEFAULT_CHARS;
    bool emitCode = false;
    bool emitJson = false;
    bool scanMode = false;
    bool useClearType = false;
    const char* outputPath = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--cleartype") == 0) {
            useClearType = true;
        } else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) outputPath = argv[++i];
        } else if (strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            const char* s = argv[++i];
            size_t len = strlen(s);
            fontName.resize(len + 1);
            size_t outLen = 0;
            mbstowcs_s(&outLen, &fontName[0], len + 1, s, _TRUNCATE);
            fontName.resize(len);
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            pointSize = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--chars") == 0 && i + 1 < argc) {
            const char* s = argv[++i];
            size_t len = strlen(s);
            chars.resize(len + 1);
            size_t outLen = 0;
            mbstowcs_s(&outLen, &chars[0], len + 1, s, _TRUNCATE);
            chars.resize(len);
        } else if (strcmp(argv[i], "--scan") == 0) {
            scanMode = true;
        } else if (strcmp(argv[i], "--code") == 0) {
            emitCode = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            emitJson = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    FILE* out = stdout;
    if (outputPath) {
        (void)remove(outputPath);  // Replace any existing file
        if (fopen_s(&out, outputPath, "w") != 0 || !out) {
            fprintf(stderr, "Failed to open %s for writing (file may be in use)\n", outputPath);
            return 1;
        }
    }

    IDWriteFactory* factory = nullptr;
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        (IUnknown**)&factory
    );
    if (FAILED(hr)) {
        fprintf(stderr, "DWriteCreateFactory failed: 0x%08X\n", (unsigned)hr);
        if (out != stdout) fclose(out);
        return 1;
    }

    IDWriteFontCollection* collection = nullptr;
    hr = factory->GetSystemFontCollection(&collection, FALSE);
    if (FAILED(hr)) {
        fprintf(stderr, "GetSystemFontCollection failed: 0x%08X\n", (unsigned)hr);
        factory->Release();
        if (out != stdout) fclose(out);
        return 1;
    }

    UINT32 familyIndex = 0;
    BOOL exists = FALSE;
    hr = collection->FindFamilyName(fontName.c_str(), &familyIndex, &exists);
    if (FAILED(hr) || !exists) {
        // Fallback: try "Square" (family name) if "Square Modern" (full face) not found
        if (fontName == L"Square Modern") {
            fontName = L"Square";
            hr = collection->FindFamilyName(fontName.c_str(), &familyIndex, &exists);
        }
        if (FAILED(hr) || !exists) {
            fprintf(stderr, "Font family not found. Tried Square Modern and Square.\n");
            collection->Release();
            factory->Release();
            if (out != stdout) fclose(out);
            return 1;
        }
    }

    IDWriteFontFamily* family = nullptr;
    hr = collection->GetFontFamily(familyIndex, &family);
    collection->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "GetFontFamily failed: 0x%08X\n", (unsigned)hr);
        factory->Release();
        if (out != stdout) fclose(out);
        return 1;
    }

    IDWriteFont* font = nullptr;
    hr = family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    family->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "GetFirstMatchingFont failed: 0x%08X\n", (unsigned)hr);
        factory->Release();
        if (out != stdout) fclose(out);
        return 1;
    }

    IDWriteFontFace* fontFace = nullptr;
    hr = font->CreateFontFace(&fontFace);
    font->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "CreateFontFace failed: 0x%08X\n", (unsigned)hr);
        factory->Release();
        if (out != stdout) fclose(out);
        return 1;
    }

    // At 96 DPI, 1 DIP = 1 pixel. Use 1.0f so texture bounds match logical cell size (MSDN: "if 96 DPI then pixelsPerDip is 1").
    const float pixelsPerDip = 1.0f;

    if (scanMode) {
        // Scan sizes 2.0 to 11.0 step 0.01, merge consecutive identical coverages into intervals
        int numSteps = (int)((SCAN_SIZE_MAX - SCAN_SIZE_MIN) / SCAN_STEP + 0.5f) + 1;
        std::vector<Interval> intervals;
        std::vector<float> prev, curr;
        float intervalStart = SCAN_SIZE_MIN;
        bool havePrev = false;

        for (int step = 0; step < numSteps; ++step) {
            float size = SCAN_SIZE_MIN + step * SCAN_STEP;
            if (size > SCAN_SIZE_MAX) break;

            size_t failedCharIndex = 0;
            HRESULT lastHr = S_OK;
            if (!computeCoveragesForSize(factory, fontFace, chars, size, pixelsPerDip, useClearType, curr, &failedCharIndex, &lastHr)) {
                wchar_t ch = failedCharIndex < chars.size() ? chars[failedCharIndex] : 0;
                fprintf(stderr, "Failed at size %.2f pt (character ", size);
                if (ch == L' ') fprintf(stderr, "' '");
                else if (ch >= 32 && ch < 127) fprintf(stderr, "'%c'", (char)ch);
                else fprintf(stderr, "U+%04X", (unsigned)ch);
                fprintf(stderr, ") HRESULT 0x%08X, skipping size\n", (unsigned)lastHr);
                if (havePrev) {
                    Interval iv;
                    iv.sizeMin = intervalStart;
                    iv.sizeMax = size - SCAN_STEP;
                    iv.coverages = prev;
                    intervals.push_back(iv);
                    intervalStart = size + SCAN_STEP;
                    havePrev = false;
                }
                continue;
            }

            if (havePrev && !coverageEqual(prev, curr)) {
                Interval iv;
                iv.sizeMin = intervalStart;
                iv.sizeMax = size - SCAN_STEP;  // previous size was last of this interval
                iv.coverages = prev;
                intervals.push_back(iv);
                intervalStart = size;
            }
            prev = curr;
            havePrev = true;
        }
        if (havePrev) {
            Interval iv;
            iv.sizeMin = intervalStart;
            iv.sizeMax = SCAN_SIZE_MAX;
            iv.coverages = prev;
            intervals.push_back(iv);
        }

        fontFace->Release();
        factory->Release();

        // Output unique intervals as JSON
        {
            char fontBuf[256] = {};
            size_t convLen = 0;
            wcstombs_s(&convLen, fontBuf, sizeof(fontBuf), fontName.c_str(), _TRUNCATE);
            fprintf(out, "{\n  \"font\": \"");
            for (char* p = fontBuf; *p; ++p) {
                if (*p == '\\' || *p == '"') fputc('\\', out);
                fputc(*p, out);
            }
            fprintf(out, "\",\n  \"sizeMin\": %.2f,\n  \"sizeMax\": %.2f,\n  \"step\": %.2f,\n  \"cleartype\": %s,\n  \"chars\": [",
                    SCAN_SIZE_MIN, SCAN_SIZE_MAX, SCAN_STEP, useClearType ? "true" : "false");
            for (size_t k = 0; k < chars.size(); ++k) {
                if (k) fprintf(out, ", ");
                fprintf(out, "%u", (unsigned)(unsigned short)chars[k]);
            }
            fprintf(out, "],\n  \"intervals\": [\n");
            for (size_t i = 0; i < intervals.size(); ++i) {
                const Interval& iv = intervals[i];
                fprintf(out, "    { \"sizeMin\": %.2f, \"sizeMax\": %.2f, \"coverages\": [", iv.sizeMin, iv.sizeMax);
                for (size_t j = 0; j < iv.coverages.size(); ++j) {
                    if (j) fprintf(out, ", ");
                    fprintf(out, "%.6f", iv.coverages[j]);
                }
                fprintf(out, "] }%s\n", (i + 1 < intervals.size()) ? "," : "");
            }
            fprintf(out, "  ]\n}\n");
        }
        if (out != stdout) fclose(out);
        return 0;
    }

    // Single-size path (same point-to-DIP as computeCoveragesForSize)
    float fontEmSizeDIP = pointSize * 96.0f / 72.0f;
    std::vector<float> coverages;
    coverages.reserve(chars.size());

    HRESULT lastHr = S_OK;
    for (size_t i = 0; i < chars.size(); ++i) {
        float c = computeCoverage(factory, fontFace, fontEmSizeDIP, chars[i], pixelsPerDip, useClearType, &lastHr);
        if (c < 0.f) {
            wchar_t ch = chars[i];
            fprintf(stderr, "Failed for character ");
            if (ch == L' ') fprintf(stderr, "' '");
            else if (ch >= 32 && ch < 127) fprintf(stderr, "'%c'", (char)ch);
            else fprintf(stderr, "U+%04X", (unsigned)ch);
            fprintf(stderr, " HRESULT 0x%08X\n", (unsigned)lastHr);
            fontFace->Release();
            factory->Release();
            if (out != stdout) fclose(out);
            return 1;
        }
        coverages.push_back(c);
    }

    fontFace->Release();
    factory->Release();

    // Output
    if (emitJson) {
        fprintf(out, "{\"font\":\"%S\",\"size\":%.2f,\"cleartype\":%s,\"chars\":[", fontName.c_str(), pointSize, useClearType ? "true" : "false");
        for (size_t i = 0; i < chars.size(); ++i) {
            if (i) fprintf(out, ",");
            fprintf(out, "%u", (unsigned)(unsigned short)chars[i]);
        }
        fprintf(out, "],\"coverages\":[");
        for (size_t i = 0; i < coverages.size(); ++i) {
            if (i) fprintf(out, ",");
            fprintf(out, "%.6f", coverages[i]);
        }
        fprintf(out, "]}\n");
    } else if (emitCode) {
        fprintf(out, "// Analytically computed for font \"%S\" @ %.1fpt%s (run tools/CharCoverage/char_coverage.exe)\n", fontName.c_str(), pointSize, useClearType ? " ClearType" : "");
        fprintf(out, "static constexpr std::array<float, %zu> _charCoverage = {\n", coverages.size());
        for (size_t i = 0; i < coverages.size(); ++i) {
            wchar_t ch = chars[i];
            if (ch == L' ') fprintf(out, "    %.6ff,  // ' '\n", coverages[i]);
            else if (ch >= 32 && ch < 127) fprintf(out, "    %.6ff,  // '%c'\n", coverages[i], (char)ch);
            else fprintf(out, "    %.6ff,  // U+%04X\n", coverages[i], (unsigned)ch);
        }
        fprintf(out, "};\n");
    } else {
        fprintf(out, "Font: %S @ %.1f pt%s\n", fontName.c_str(), pointSize, useClearType ? "  [ClearType]" : "");
        fprintf(out, "%-8s %s\n", "Char", "Coverage");
        fprintf(out, "-------- ----------\n");
        for (size_t i = 0; i < coverages.size(); ++i) {
            wchar_t ch = chars[i];
            if (ch == L' ') fprintf(out, "%-8s %.6f\n", "' '", coverages[i]);
            else if (ch >= 32 && ch < 127) fprintf(out, "%-8c %.6f\n", (char)ch, coverages[i]);
            else fprintf(out, "U+%04X   %.6f\n", (unsigned)ch, coverages[i]);
        }
    }

    if (out != stdout) fclose(out);
    return 0;
}
