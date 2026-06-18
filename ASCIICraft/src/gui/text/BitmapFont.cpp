#include <ASCIICraft/gui/text/BitmapFont.hpp>

#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/util/Logger.hpp>

namespace gui::text {

std::shared_ptr<BitmapFont> LoadBitmapFont(
    std::shared_ptr<ASCIIgL::TextureArray> textureArray,
    int startingLayer,
    std::string_view characters
) {
    if (!textureArray || !textureArray->IsValid()) {
        ASCIIgL::Logger::Error("LoadBitmapFont: invalid texture array");
        return nullptr;
    }

    const int tileSize = textureArray->GetTileSize();
    const int layerCount = textureArray->GetLayerCount();
    if (tileSize <= 0 || layerCount <= 0) {
        ASCIIgL::Logger::Error("LoadBitmapFont: texture array has no layers");
        return nullptr;
    }

    if (startingLayer < 0 || startingLayer >= layerCount) {
        ASCIIgL::Logger::Error("LoadBitmapFont: startingLayer out of range");
        return nullptr;
    }

    if (static_cast<std::size_t>(startingLayer + characters.size()) > static_cast<std::size_t>(layerCount)) {
        ASCIIgL::Logger::Error("LoadBitmapFont: character list exceeds available layers");
        return nullptr;
    }

    auto font = std::make_shared<BitmapFont>();
    font->textureArray = std::move(textureArray);
    font->lineHeight = static_cast<float>(tileSize);

    const float glyphSize = static_cast<float>(tileSize);
    for (std::size_t i = 0; i < characters.size(); ++i) {
        const char32_t codepoint = static_cast<unsigned char>(characters[i]);
        GlyphMetrics metrics{};
        metrics.layer = startingLayer + static_cast<int>(i);
        metrics.advance = glyphSize;
        metrics.width = glyphSize;
        metrics.height = glyphSize;
        metrics.valid = true;
        font->glyphs[codepoint] = metrics;
    }

    if (font->glyphs.find(font->fallbackCodepoint) == font->glyphs.end() && !characters.empty()) {
        font->fallbackCodepoint = static_cast<unsigned char>(characters.front());
    }

    return font;
}

} // namespace gui::text
