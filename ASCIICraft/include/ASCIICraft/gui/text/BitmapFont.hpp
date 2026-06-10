#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ASCIIgL { class Texture; class TextureArray; }

namespace gui::text {

struct GlyphMetrics {
    int layer = -1;

    float advance = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool valid = false;
};

struct BitmapFont {
    std::shared_ptr<ASCIIgL::Texture> texture;
    std::shared_ptr<ASCIIgL::TextureArray> textureArray;

    int atlasWidth = 0;
    int atlasHeight = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int columns = 0;
    int rows = 0;

    float lineHeight = 0.0f;
    float letterSpacing = 0.0f;
    float wordSpacing = 0.0f;

    char32_t fallbackCodepoint = U'?';
    std::unordered_map<char32_t, GlyphMetrics> glyphs;
};

} // namespace gui::text
