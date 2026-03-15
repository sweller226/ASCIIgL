#include <ASCIIgL/util/FontAtlasBuilder.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")

#include <algorithm>
#include <cstring>
#include <vector>

namespace ASCIIgL {

namespace {

// Match CharCoverage (coverage_cleartype.json): 1 pixel per DIP, no supersample.
// This way the atlas glyphs are drawn exactly like the tool that produces the coverage JSON.
// See docs/FontAtlas-vs-CoverageJSON.md.
const float PIXELS_PER_DIP = 1.0f;
const bool USE_CLEARTYPE = true;   // Match CharCoverage / terminal

// Get font face from system collection (Square Modern or Square).
IDWriteFontFace* GetFontFaceFromSystem(IDWriteFactory* factory, HRESULT* outHr) {
    IDWriteFontCollection* collection = nullptr;
    HRESULT hr = factory->GetSystemFontCollection(&collection, FALSE);
    if (FAILED(hr)) {
        if (outHr) *outHr = hr;
        return nullptr;
    }

    const wchar_t* names[] = { L"Square Modern", L"Square" };
    IDWriteFontFace* fontFace = nullptr;

    for (const wchar_t* fontName : names) {
        UINT32 familyIndex = 0;
        BOOL exists = FALSE;
        hr = collection->FindFamilyName(fontName, &familyIndex, &exists);
        if (FAILED(hr) || !exists) continue;

        IDWriteFontFamily* family = nullptr;
        hr = collection->GetFontFamily(familyIndex, &family);
        if (FAILED(hr)) continue;

        IDWriteFont* font = nullptr;
        hr = family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
        family->Release();
        if (FAILED(hr)) continue;

        hr = font->CreateFontFace(&fontFace);
        font->Release();
        if (SUCCEEDED(hr) && fontFace) {
            collection->Release();
            if (outHr) *outHr = S_OK;
            return fontFace;
        }
    }

    collection->Release();
    if (outHr) *outHr = E_FAIL;
    return nullptr;
}

// Rasterize one glyph into a fixed cell-sized RGBA8 buffer (R=G=B=255, A=alpha).
// Centers the glyph in the cell; pads with 0.
bool RasterizeGlyphToCell(
    IDWriteFactory* factory,
    IDWriteFontFace* fontFace,
    wchar_t ch,
    float fontEmSizeDIP,
    int cellW,
    int cellH,
    std::vector<uint8_t>& outRGBA,
    HRESULT* outHr
) {
    outRGBA.resize(static_cast<size_t>(cellW) * static_cast<size_t>(cellH) * 4u);
    std::fill(outRGBA.begin(), outRGBA.end(), 0u);

    UINT32 codePoint = static_cast<UINT32>(static_cast<unsigned short>(ch));
    UINT16 glyphIndex = 0;
    HRESULT hr = fontFace->GetGlyphIndices(&codePoint, 1, &glyphIndex);
    if (FAILED(hr)) {
        if (outHr) *outHr = hr;
        return false;
    }

    DWRITE_FONT_METRICS fontMetrics = {};
    fontFace->GetMetrics(&fontMetrics);
    DWRITE_GLYPH_METRICS glyphMetrics = {};
    hr = fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics);
    if (FAILED(hr)) {
        if (outHr) *outHr = hr;
        return false;
    }

    float advance = static_cast<float>(glyphMetrics.advanceWidth) * fontEmSizeDIP / static_cast<float>(fontMetrics.designUnitsPerEm);

    DWRITE_GLYPH_RUN run = {};
    run.fontFace = fontFace;
    run.fontEmSize = fontEmSizeDIP;
    run.glyphCount = 1;
    run.glyphIndices = &glyphIndex;
    run.glyphAdvances = &advance;
    run.glyphOffsets = nullptr;
    run.isSideways = FALSE;
    run.bidiLevel = 0;

    DWRITE_RENDERING_MODE renderingMode = USE_CLEARTYPE ? DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL : DWRITE_RENDERING_MODE_ALIASED;
    IDWriteGlyphRunAnalysis* analysis = nullptr;
    hr = factory->CreateGlyphRunAnalysis(
        &run,
        PIXELS_PER_DIP,
        nullptr,
        renderingMode,
        DWRITE_MEASURING_MODE_NATURAL,
        0.0f, 0.0f,
        &analysis
    );
    if (FAILED(hr) || !analysis) {
        if (outHr) *outHr = hr;
        return false;
    }

    DWRITE_TEXTURE_TYPE textureType = USE_CLEARTYPE ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
    RECT bounds = {};
    hr = analysis->GetAlphaTextureBounds(textureType, &bounds);
    if (FAILED(hr)) {
        analysis->Release();
        if (outHr) *outHr = hr;
        return false;
    }

    int w = bounds.right - bounds.left;
    int h = bounds.bottom - bounds.top;

    if (w <= 0 || h <= 0) {
        analysis->Release();
        if (outHr) *outHr = S_OK;
        return true; // empty glyph (e.g. space) - cell already zeroed
    }

    size_t bytesPerPixel = USE_CLEARTYPE ? 3u : 1u;
    size_t bufferSize = static_cast<size_t>(w) * static_cast<size_t>(h) * bytesPerPixel;
    std::vector<BYTE> buffer(bufferSize);
    hr = analysis->CreateAlphaTexture(textureType, &bounds, buffer.data(), static_cast<UINT32>(bufferSize));
    analysis->Release();
    if (FAILED(hr)) {
        if (outHr) *outHr = hr;
        return false;
    }

    // 1:1 copy from DirectWrite buffer into cell (same as CharCoverage pipeline); center in cell
    int offsetX = (cellW - w) / 2;
    int offsetY = (cellH - h) / 2;

    for (int sy = 0; sy < h; ++sy) {
        int cellY = offsetY + sy;
        if (cellY < 0 || cellY >= cellH) continue;
        for (int sx = 0; sx < w; ++sx) {
            int cellX = offsetX + sx;
            if (cellX < 0 || cellX >= cellW) continue;

            size_t srcIdx = (static_cast<size_t>(sy) * static_cast<size_t>(w) + static_cast<size_t>(sx)) * bytesPerPixel;
            size_t dstIdx = (static_cast<size_t>(cellY) * static_cast<size_t>(cellW) + static_cast<size_t>(cellX)) * 4u;
            if (USE_CLEARTYPE) {
                outRGBA[dstIdx + 0] = buffer[srcIdx + 0];
                outRGBA[dstIdx + 1] = buffer[srcIdx + 1];
                outRGBA[dstIdx + 2] = buffer[srcIdx + 2];
            } else {
                uint8_t a = buffer[srcIdx];
                outRGBA[dstIdx + 0] = a;
                outRGBA[dstIdx + 1] = a;
                outRGBA[dstIdx + 2] = a;
            }
            outRGBA[dstIdx + 3] = 255;
        }
    }

    if (outHr) *outHr = S_OK;
    return true;
}

} // namespace

FontAtlasBuildResult BuildFontAtlasFromDirectWrite(
    const wchar_t* fontPath,
    float pointSize,
    const wchar_t* charRamp,
    int cellPixelsX,
    int cellPixelsY
) {
    FontAtlasBuildResult result;
    result.cellPixelsX = cellPixelsX;
    result.cellPixelsY = cellPixelsY;

    if (!charRamp || cellPixelsX <= 0 || cellPixelsY <= 0) {
        Logger::Error(L"FontAtlasBuilder: invalid input (charRamp, cell size)");
        return result;
    }

    size_t rampLen = 0;
    while (charRamp[rampLen]) ++rampLen;
    if (rampLen == 0) {
        Logger::Error(L"FontAtlasBuilder: empty char ramp");
        return result;
    }

    IDWriteFactory* factory = nullptr;
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&factory)
    );
    if (FAILED(hr)) {
        Logger::Error(L"FontAtlasBuilder: DWriteCreateFactory failed");
        return result;
    }

    IDWriteFontFace* fontFace = GetFontFaceFromSystem(factory, &hr);
    (void)fontPath; // TODO: load from file via custom loader if needed
    if (!fontFace) {
        Logger::Error(L"FontAtlasBuilder: could not get font face (Square Modern / Square)");
        factory->Release();
        return result;
    }

    float fontEmSizeDIP = pointSize * 96.0f / 72.0f;
    if (fontEmSizeDIP < 1.0f) fontEmSizeDIP = 1.0f;

    result.slices.reserve(rampLen);
    for (size_t i = 0; i < rampLen; ++i) {
        std::vector<uint8_t> cellRGBA;
        if (!RasterizeGlyphToCell(factory, fontFace, charRamp[i], fontEmSizeDIP, cellPixelsX, cellPixelsY, cellRGBA, &hr)) {
            Logger::Warning(L"FontAtlasBuilder: failed to rasterize glyph at index " + std::to_wstring(i));
            cellRGBA.resize(static_cast<size_t>(cellPixelsX) * static_cast<size_t>(cellPixelsY) * 4u, 0u);
        }
        result.slices.push_back(std::move(cellRGBA));
    }

    fontFace->Release();
    factory->Release();

    result.glyphCount = static_cast<int>(result.slices.size());
    result.success = true;
    return result;
}

} // namespace ASCIIgL
