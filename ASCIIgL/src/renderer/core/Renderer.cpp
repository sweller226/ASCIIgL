#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <Windows.h>

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/CoverageJson.hpp>
#include <ASCIIgL/engine/Collision.hpp>

#include <ASCIIgL/util/MathUtil.hpp>
#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include "renderer/resources/MaterialInternal.hpp"
#include "renderer/core/RendererImpl.hpp"
#include "renderer/resources/ShaderInternal.hpp"

namespace ASCIIgL {

using ColorLUTState = Renderer::Impl::ColorLUTState;

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}

// Core lifecycle and non-draw functionality for Renderer lives here.
// Draw-call related methods have been moved to Renderer_Draw.cpp.

// =============================================================================
// TRIANGLE RASTERIZATION - WIREFRAME (PIXEL BUFFER)
// =============================================================================

void Renderer::DrawTriangleWireframePxBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const WCHAR pixel_type, const unsigned short col) {
    // RENDERING LINES BETWEEN VERTICES
    DrawLinePxBuff((int) vert1.x, (int) vert1.y, (int) vert2.x, (int) vert2.y, pixel_type, col);
    DrawLinePxBuff((int) vert2.x, (int) vert2.y, (int) vert3.x, (int) vert3.y, pixel_type, col);
    DrawLinePxBuff((int) vert3.x, (int) vert3.y, (int) vert1.x, (int) vert1.y, pixel_type, col);
}

// =============================================================================
// LINE DRAWING - PIXEL BUFFER
// =============================================================================

void Renderer::DrawLinePxBuff(const int x1, const int y1, const int x2, const int y2, const WCHAR pixel_type, const unsigned short col) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int incx = (x2 > x1) ? 1 : -1;
    int incy = (y2 > y1) ? 1 : -1;
    int x = x1, y = y1;

    Screen::GetInst().PlotPixel(x, y, pixel_type, col);

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            Screen::GetInst().PlotPixel(x, y, pixel_type, col);
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            Screen::GetInst().PlotPixel(x, y, pixel_type, col);
        }
    }
}

void Renderer::DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col) {
    // Bresenham's line algorithm with tile bounds clipping
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int incx = (x1 > x0) ? 1 : -1;
    int incy = (y1 > y0) ? 1 : -1;
    int x = x0, y = y0;

    // Plot first pixel if within bounds
    if (x >= minX && x < maxX && y >= minY && y < maxY) {
        Screen::GetInst().PlotPixel(x, y, pixel_type, col);
    }

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                Screen::GetInst().PlotPixel(x, y, pixel_type, col);
            }
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                Screen::GetInst().PlotPixel(x, y, pixel_type, col);
            }
        }
    }
}

void Renderer::DrawScreenBorderPxBuff(const unsigned short col) {
    wchar_t borderChar = impl_->_charRamp.empty() ? L'#' : impl_->_charRamp.back();
    DrawLinePxBuff(0, 0, Screen::GetInst().GetWidth() - 1, 0, borderChar, col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, 0, Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, borderChar, col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, 0, Screen::GetInst().GetHeight() - 1, borderChar, col);
    DrawLinePxBuff(0, 0, 0, Screen::GetInst().GetHeight() - 1, borderChar, col);
}

// =============================================================================
// CHAR COVERAGE (loaded from JSON to match char_coverage tool)
// =============================================================================

bool Renderer::LoadCharCoverageFromJson(const wchar_t* charRamp, int charRampCount) {
    impl_->_charRamp.clear();
    impl_->_charCoverage.clear();
    impl_->_colorLUTState = ColorLUTState::NotComputed;
    impl_->_lutGpuResourcesDirty = true;

    const bool useCustomRamp = (charRamp && *charRamp);
    float fontSize = Screen::GetInst().GetFontSize();

    CoverageInterval interval;
    if (!CoverageJson::GetIntervalForFontSize(fontSize, interval)) {
        Logger::Error("[Renderer] coverage_cleartype.json not found or has no interval for font size " +
                      std::to_string(fontSize) + ".");
        return false;
    }

    if (interval.chars.empty() || interval.chars.size() != interval.coverages.size()) {
        Logger::Error("[Renderer] coverage JSON chars/coverages missing or mismatched.");
        return false;
    }

    if (useCustomRamp) {
        // Custom ramp: look up each requested char's coverage from the JSON interval.
        std::unordered_map<unsigned, float> charToCoverage;
        for (size_t i = 0; i < interval.chars.size(); ++i)
            charToCoverage[interval.chars[i]] = interval.coverages[i];

        for (const wchar_t* p = charRamp; *p; ++p) {
            unsigned cp = static_cast<unsigned>(*p);
            auto it = charToCoverage.find(cp);
            if (it == charToCoverage.end()) {
                Logger::Error("[Renderer] Custom charRamp includes codepoint " + std::to_string(cp) +
                              " which is not in coverage_cleartype.json.");
                impl_->_charRamp.clear();
                impl_->_charCoverage.clear();
                return false;
            }
            impl_->_charRamp.push_back(*p);
            impl_->_charCoverage.push_back(it->second);
        }
    } else {
        // Default: use the JSON chars array paired with this interval's coverages.
        impl_->_charRamp.reserve(interval.chars.size());
        impl_->_charCoverage.reserve(interval.coverages.size());
        for (size_t i = 0; i < interval.chars.size(); ++i) {
            impl_->_charRamp.push_back(static_cast<wchar_t>(interval.chars[i]));
            impl_->_charCoverage.push_back(interval.coverages[i]);
        }
    }

    if (impl_->_charRamp.empty() || impl_->_charRamp.size() != impl_->_charCoverage.size()) {
        Logger::Error("[Renderer] Char ramp empty after loading coverage JSON.");
        impl_->_charRamp.clear();
        impl_->_charCoverage.clear();
        return false;
    }

    // Sort ramp by coverage ascending (low-coverage chars first, high last)
    const size_t n = impl_->_charRamp.size();
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), size_t(0));
    std::sort(order.begin(), order.end(), [this](size_t a, size_t b) { return impl_->_charCoverage[a] < impl_->_charCoverage[b]; });
    std::vector<wchar_t> sortedRamp(n);
    std::vector<float> sortedCoverage(n);
    for (size_t i = 0; i < n; ++i) {
        sortedRamp[i] = impl_->_charRamp[order[i]];
        sortedCoverage[i] = impl_->_charCoverage[order[i]];
    }
    impl_->_charRamp = std::move(sortedRamp);
    impl_->_charCoverage = std::move(sortedCoverage);

    // When using JSON chars (no custom string), optionally subsample if charRampCount > 0
    if (!useCustomRamp && charRampCount > 0) {
        const size_t n = impl_->_charRamp.size();
        const int K = charRampCount;
        if (n > 0 && static_cast<size_t>(K) < n) {
            const float c_min = impl_->_charCoverage[0];
            const float c_max = impl_->_charCoverage[n - 1];
            std::vector<size_t> chosen;
            chosen.reserve(static_cast<size_t>(K));
            for (int i = 0; i < K; ++i) {
                float target = (K > 1) ? (c_min + (c_max - c_min) * static_cast<float>(i) / (K - 1)) : (0.5f * (c_min + c_max));
                size_t best = 0;
                float bestDist = std::abs(impl_->_charCoverage[0] - target);
                for (size_t j = 1; j < n; ++j) {
                    float d = std::abs(impl_->_charCoverage[j] - target);
                    if (d < bestDist) {
                        bestDist = d;
                        best = j;
                    }
                }
                // If this index was already chosen, pick next closest that isn't chosen (keep distinct chars)
                auto alreadyChosen = [&chosen](size_t idx) {
                    return std::find(chosen.begin(), chosen.end(), idx) != chosen.end();
                };
                if (alreadyChosen(best)) {
                    bestDist = std::numeric_limits<float>::max();
                    for (size_t j = 0; j < n; ++j) {
                        if (alreadyChosen(j)) continue;
                        float d = std::abs(impl_->_charCoverage[j] - target);
                        if (d < bestDist) {
                            bestDist = d;
                            best = j;
                        }
                    }
                }
                chosen.push_back(best);
            }
            // Sort chosen by coverage so ramp stays ascending
            std::sort(chosen.begin(), chosen.end(), [this](size_t a, size_t b) { return impl_->_charCoverage[a] < impl_->_charCoverage[b]; });
            std::vector<wchar_t> subRamp(static_cast<size_t>(K));
            std::vector<float> subCoverage(static_cast<size_t>(K));
            for (int i = 0; i < K; ++i) {
                subRamp[static_cast<size_t>(i)] = impl_->_charRamp[chosen[static_cast<size_t>(i)]];
                subCoverage[static_cast<size_t>(i)] = impl_->_charCoverage[chosen[static_cast<size_t>(i)]];
            }
            impl_->_charRamp = std::move(subRamp);
            impl_->_charCoverage = std::move(subCoverage);
        }
    }

    Logger::Info("[Renderer] Loaded char coverage for font size " + std::to_string(fontSize) + " (" + std::to_string(impl_->_charRamp.size()) + " glyphs)");
    for (size_t i = 0; i < impl_->_charRamp.size() && i < impl_->_charCoverage.size(); ++i) {
        wchar_t ch = impl_->_charRamp[i];
        float cov = impl_->_charCoverage[i];
        std::wstring charDesc;
        if (ch == L' ') charDesc = L"' '";
        else if (ch == L'"') charDesc = L"'\"'";
        else if (ch == L'\\') charDesc = L"'\\\\'";
        else if (ch >= 32 && ch < 127) charDesc = std::wstring(L"'") + ch + L"'";
        else {
            charDesc = L"U+";
            wchar_t hex[8];
            swprintf_s(hex, L"%04X", (unsigned)(unsigned short)ch);
            charDesc += hex;
        }
        Logger::Debug(L"[Renderer] char coverage " + charDesc + L" => " + std::to_wstring(cov));
    }
    return true;
}

// =============================================================================
// COLOR LOOKUP TABLE (LUT)
// =============================================================================

size_t Renderer::MonochromeLuminanceToIndex(float L) const {
    // Monochrome luminance samples were built by linearly interpolating between
    // lowLinear and highLinear and then taking LinearRGB_Luminance, so the
    // luminance samples are effectively uniform between [L_min, L_max].
    // We can therefore invert using a simple affine mapping + round,
    // clamped to [0, Renderer::Impl::_monochromeLUTSize-1).
    const float Lmin = impl_->_monochromeLUT.front().first;
    const float Lmax = impl_->_monochromeLUT.back().first;
    if (L <= Lmin) return 0;
    if (L >= Lmax) return Renderer::Impl::_monochromeLUTSize - 1;

    const float t = (L - Lmin) / (Lmax - Lmin); // in (0,1)
    float fIdx = t * static_cast<float>(Renderer::Impl::_monochromeLUTSize - 1);
    // Round to nearest index
    size_t idx = static_cast<size_t>(std::floor(fIdx + 0.5f));
    if (idx >= Renderer::Impl::_monochromeLUTSize) idx = Renderer::Impl::_monochromeLUTSize - 1;
    return idx;
}

void Renderer::PrecomputeColorLUT() {
    Palette& palette = Screen::GetInst().GetPalette();
    if (impl_->_colorLUTState != ColorLUTState::NotComputed) return;
    if (impl_->_charRamp.empty() || impl_->_charRamp.size() != impl_->_charCoverage.size()) {
        Logger::Error("[Renderer] Char ramp empty or size mismatch; cannot precompute color LUT without coverage JSON.");
        return;
    }

    if (Screen::GetInst().IsMonochromePalette()) {
        PrecomputeMonochromeColorLUT(palette);
        return;
    }

    PrecomputeMultiColorLUT(palette);
}

void Renderer::PrecomputeMonochromeColorLUT(Palette& palette) {
    Logger::Info("[Renderer] Precomputing monochrome color LUT...");

    impl_->_colorLUTState = ColorLUTState::Monochrome;

    // ===== MONOCHROME: 1-D LUT sweep from lowest to highest palette color (0..1023) =====
    unsigned int minLumIdx = palette.GetMinLumIdx();
    unsigned int maxLumIdx = palette.GetMaxLumIdx();
    glm::vec3 lowRGB = palette.GetRGBNormalized(minLumIdx);
    glm::vec3 highRGB = palette.GetRGBNormalized(maxLumIdx);
    glm::vec3 lowLinear = PaletteUtil::sRGB1ToLinear1(lowRGB);
    glm::vec3 highLinear = PaletteUtil::sRGB1ToLinear1(highRGB);

    const float denom = (Renderer::Impl::_monochromeLUTSize > 1) ? static_cast<float>(Renderer::Impl::_monochromeLUTSize - 1) : 1.0f;
    for (size_t i = 0; i < Renderer::Impl::_monochromeLUTSize; ++i) {
        float t = static_cast<float>(i) / denom;
        glm::vec3 targetLinear = glm::mix(lowLinear, highLinear, t);
        float targetLuminance = PaletteUtil::LinearRGB_Luminance(targetLinear);

        float minError = FLT_MAX;
        int bestFgIndex = 0, bestBgIndex = 0, bestCharIndex = 0;

        for (int fgIdx = 0; fgIdx < static_cast<int>(palette.COLOR_COUNT); ++fgIdx) {
            float fgLuminance = palette.GetLuminance(fgIdx);
            for (int bgIdx = 0; bgIdx < static_cast<int>(palette.COLOR_COUNT); ++bgIdx) {
                float bgLuminance = palette.GetLuminance(bgIdx);
                for (int charIdx = 0; charIdx < static_cast<int>(impl_->_charRamp.size()); ++charIdx) {
                    float coverage = impl_->_charCoverage[charIdx];
                    float simLum = coverage * fgLuminance + (1.0f - coverage) * bgLuminance;
                    float error = std::abs(targetLuminance - simLum);
                    if (error < minError) {
                        minError = error;
                        bestFgIndex = fgIdx;
                        bestBgIndex = bgIdx;
                        bestCharIndex = charIdx;
                    }
                }
            }
        }

        wchar_t glyph = impl_->_charRamp[bestCharIndex];
        const unsigned short combinedColor = static_cast<unsigned short>(
            ((bestBgIndex & 0xF) << 4) | (bestFgIndex & 0xF));
        impl_->_monochromeLUT[i] = std::make_pair(targetLuminance, ScreenPixel{ glyph, combinedColor });
    }

    // Make sure LUT is sorted by target luminance (ascending)
    std::sort(impl_->_monochromeLUT.begin(), impl_->_monochromeLUT.end(),
              [](const std::pair<float, ScreenPixel>& a, const std::pair<float, ScreenPixel>& b) {
                  return a.first < b.first;
              });
    impl_->_lutGpuResourcesDirty = true;
}

void Renderer::PrecomputeMultiColorLUT(Palette& palette) {
    Logger::Info("[Renderer] Precomputing multi-color color LUT...");

    impl_->_colorLUTState = ColorLUTState::MultiColor;

    struct LutCandidate {
        glm::vec3 oklab;
        uint8_t fgIndex;
        uint8_t bgIndex;
        uint8_t charIndex;
    };

    const int colorCount = static_cast<int>(palette.COLOR_COUNT);
    const int charCount = static_cast<int>(impl_->_charRamp.size());

    std::array<glm::vec3, Palette::COLOR_COUNT> paletteLinear{};
    for (int i = 0; i < colorCount; ++i) {
        paletteLinear[static_cast<size_t>(i)] = PaletteUtil::sRGB1ToLinear1(palette.GetRGBNormalized(i));
    }

    std::vector<LutCandidate> candidates;
    candidates.reserve(static_cast<size_t>(colorCount) * static_cast<size_t>(colorCount) * static_cast<size_t>(charCount));
    for (int fgIdx = 0; fgIdx < colorCount; ++fgIdx) {
        const glm::vec3& fgLinear = paletteLinear[static_cast<size_t>(fgIdx)];
        for (int bgIdx = 0; bgIdx < colorCount; ++bgIdx) {
            const glm::vec3& bgLinear = paletteLinear[static_cast<size_t>(bgIdx)];
            for (int charIdx = 0; charIdx < charCount; ++charIdx) {
                const float coverage = impl_->_charCoverage[static_cast<size_t>(charIdx)];
                const glm::vec3 simLinear = coverage * fgLinear + (1.0f - coverage) * bgLinear;
                candidates.push_back({
                    PaletteUtil::Linear1ToOklab(simLinear),
                    static_cast<uint8_t>(fgIdx),
                    static_cast<uint8_t>(bgIdx),
                    static_cast<uint8_t>(charIdx),
                });
            }
        }
    }

    const int depth = static_cast<int>(Renderer::Impl::_rgbLUTDepth);
    const int depthSq = depth * depth;
    const int totalVoxels = depth * depthSq;
    const float invPaletteDepth = 1.0f / static_cast<float>(Renderer::Impl::_rgbLUTMaxIndex);
    const LutCandidate* const candidatesData = candidates.data();
    const size_t candidateCount = candidates.size();

    oneapi::tbb::parallel_for(
        oneapi::tbb::blocked_range<int>(0, totalVoxels),
        [&](const oneapi::tbb::blocked_range<int>& range) {
            for (int index = range.begin(); index != range.end(); ++index) {
                const int r = index / depthSq;
                const int rem = index % depthSq;
                const int g = rem / depth;
                const int b = rem % depth;

                const glm::vec3 targetSRGB(
                    r * invPaletteDepth,
                    g * invPaletteDepth,
                    b * invPaletteDepth);
                const glm::vec3 targetOklab = PaletteUtil::Linear1ToOklab(
                    PaletteUtil::sRGB1ToLinear1(targetSRGB));

                float minError = FLT_MAX;
                int bestFgIndex = 0;
                int bestBgIndex = 0;
                int bestCharIndex = 0;

                for (size_t ci = 0; ci < candidateCount; ++ci) {
                    const LutCandidate& cand = candidatesData[ci];
                    const glm::vec3 diff = targetOklab - cand.oklab;
                    const float error = glm::dot(diff, diff);
                    if (error < minError) {
                        minError = error;
                        bestFgIndex = cand.fgIndex;
                        bestBgIndex = cand.bgIndex;
                        bestCharIndex = cand.charIndex;
                    }
                }

                const wchar_t glyph = impl_->_charRamp[static_cast<size_t>(bestCharIndex)];
                const unsigned short combinedColor = static_cast<unsigned short>(
                    ((bestBgIndex & 0xF) << 4) | (bestFgIndex & 0xF));
                impl_->_colorLUT[static_cast<size_t>(index)] = {glyph, combinedColor};
            }
        });

    impl_->_lutGpuResourcesDirty = true;
    Logger::Info("[Renderer] Multi-color color LUT precompute complete.");
}

ScreenPixel Renderer::GetCharInfo(const glm::ivec3& rgb) {
    if (impl_->_colorLUTState == ColorLUTState::NotComputed) {
        PrecomputeColorLUT();
    }

    if (impl_->_colorLUTState == ColorLUTState::Monochrome) {
        float L = PaletteUtil::sRGB255_Luminance(rgb);
        size_t idx = MonochromeLuminanceToIndex(L);
        return impl_->_monochromeLUT[idx].second;
    }

    auto toBucket = [](int v) {
        return (v * static_cast<int>(Renderer::Impl::_rgbLUTMaxIndex) + 127) / 255;
    };
    const int r = toBucket(rgb.r), g = toBucket(rgb.g), b = toBucket(rgb.b);
    const int index = (r * Renderer::Impl::_rgbLUTDepth * Renderer::Impl::_rgbLUTDepth) + (g * Renderer::Impl::_rgbLUTDepth) + b;
    return impl_->_colorLUT[index];
}

// =============================================================================
// RENDER SETTINGS - RENDERING OPTIONS
// =============================================================================

void Renderer::SetWireframe(bool wireframe) {
    impl_->_wireframe = wireframe;
    InvalidateBoundState();
}

bool Renderer::GetWireframe() const {
    return impl_->_wireframe;
}

void Renderer::SetBackfaceCulling(bool backfaceCulling) {
    impl_->_backface_culling = backfaceCulling;
    InvalidateBoundState();
}

bool Renderer::GetBackfaceCulling() const {
    return impl_->_backface_culling;
}

void Renderer::SetCCW(bool ccw) {
    impl_->_ccw = ccw;
    InvalidateBoundState();
}

bool Renderer::GetCCW() const {
    return impl_->_ccw;
}

void Renderer::SetBackgroundCol(const glm::ivec3& col) {
    impl_->_background_col = glm::clamp(col, glm::ivec3(0), glm::ivec3(255));
}

glm::ivec3 Renderer::GetBackgroundCol() const {
    return impl_->_background_col;
}

void Renderer::SetMaxAnisotropy(int level) {
    if (level <= 1) level = 1;
    else if (level <= 2) level = 2;
    else if (level <= 4) level = 4;
    else if (level <= 8) level = 8;
    else level = 16;

    if (level == impl_->_maxAnisotropy) return;

    impl_->_maxAnisotropy = level;

    if (impl_->_initialized) {
        impl_->_samplerAnisotropic.Reset();
        if (!CreateAnisotropicSampler()) {
            Logger::Error("[Renderer] Failed to recreate anisotropic sampler at " + std::to_string(impl_->_maxAnisotropy) + "x");
        }
    }
}

int Renderer::GetMaxAnisotropy() const {
    return impl_->_maxAnisotropy;
}

// =========================================================================
// Custom Shader/Material System Implementation
// =========================================================================

void Renderer::BindShaderProgram(ShaderProgram* program) {
    if (!impl_->_initialized) return;
    
    impl_->_boundShaderProgram = program;
    
    if (program && program->IsValid()) {
        // Bind custom shaders and input layout
        impl_->_context->VSSetShader(program->_vertexShader->_impl->vertexShader.Get(), nullptr, 0);
        impl_->_context->PSSetShader(program->_pixelShader->_impl->pixelShader.Get(), nullptr, 0);
        impl_->_context->IASetInputLayout(program->_impl->inputLayout.Get());
    } else {
        // Revert to default shaders
        UnbindShaderProgram();
    }
}

void Renderer::UnbindShaderProgram() {
    if (!impl_->_initialized) return;
    
    impl_->_boundShaderProgram = nullptr;
    impl_->_boundMaterial = nullptr;
}

void Renderer::BindMaterial(Material* material) {
    if (!impl_->_initialized || !material) return;
    
    impl_->_boundMaterial = material;

    material->UpdateConstantBufferData();
    
    // Bind shader program
    auto program = material->GetShaderProgram();
    if (program) {
        BindShaderProgram(program.get());
    }
    
    // Bind textures (material's per-slot sampler type is used).
    // Unbind empty slots so a prior material cannot leave stale SRVs on the same register.
    for (const auto& slot : material->_textureSlots) {
        if (slot.texture) {
            BindTexture(slot.texture, slot.slot, slot.samplerType);
        } else if (slot.textureArray) {
            BindTextureArray(slot.textureArray, slot.slot, slot.samplerType);
        } else {
            UnbindTexture(static_cast<int>(slot.slot));
        }
    }
}

void Renderer::UploadMaterialConstants(Material* material) {
    if (!material || !impl_->_initialized) return;
    
    auto program = material->GetShaderProgram();
    if (!program) return;
    
    const auto& layout = program->GetUniformLayout();
    if (layout.GetSize() == 0) return;
    
    // Create or update material's GPU constant buffer
    if (!material->_impl->constantBufferInitialized) {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = layout.GetSize();
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        HRESULT hr = impl_->_device->CreateBuffer(&cbDesc, nullptr, &material->_impl->constantBuffer);
        if (FAILED(hr)) {
            Logger::Error("Failed to create material constant buffer");
            return;
        }
        material->_impl->constantBufferInitialized = true;
    }
    
    // Map and upload data
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = impl_->_context->Map(material->_impl->constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, material->_constantBufferData.data(), layout.GetSize());
        impl_->_context->Unmap(material->_impl->constantBuffer.Get(), 0);
    }
    
    // Bind to both vertex and pixel shader stages
    impl_->_context->VSSetConstantBuffers(0, 1, material->_impl->constantBuffer.GetAddressOf());
    impl_->_context->PSSetConstantBuffers(0, 1, material->_impl->constantBuffer.GetAddressOf());
}

ShaderProgram* Renderer::GetBoundShaderProgram() const {
    return impl_ ? impl_->_boundShaderProgram : nullptr;
}

void Renderer::SetDitheringEnabled(bool enabled) {
    if (!impl_ || impl_->_ditheringEnabled == enabled) return;
    impl_->_ditheringEnabled = enabled;
    impl_->_lutGpuResourcesDirty = true;
}

bool Renderer::GetDitheringEnabled() const {
    return impl_ && impl_->_ditheringEnabled;
}

} // namespace ASCIIgL