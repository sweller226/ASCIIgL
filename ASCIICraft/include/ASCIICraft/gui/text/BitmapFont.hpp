#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace ASCIIgL { class TextureArray; }

namespace gui::text {

struct GlyphMetrics {
    int layer = -1;

    float advance = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool valid = false;
};

struct BitmapFont {
    std::shared_ptr<ASCIIgL::TextureArray> textureArray;

    float lineHeight = 0.0f;
    float letterSpacing = 0.0f;
    float wordSpacing = 0.0f;

    char32_t fallbackCodepoint = U'?';
    std::unordered_map<char32_t, GlyphMetrics> glyphs;
};

/// Builds a monospace BitmapFont by mapping \p characters to consecutive texture-array layers
/// beginning at \p startingLayer (atlas order: left-to-right, top-to-bottom).
std::shared_ptr<BitmapFont> LoadBitmapFont(
    std::shared_ptr<ASCIIgL::TextureArray> textureArray,
    int startingLayer,
    std::string_view characters
);

} // namespace gui::text
