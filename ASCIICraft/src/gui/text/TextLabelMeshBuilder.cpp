#include <ASCIICraft/gui/text/TextLabelMeshBuilder.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <algorithm>
#include <vector>

namespace gui::text {

namespace {

const GlyphMetrics& ResolveGlyph(const BitmapFont& font, char32_t cp) {
    auto it = font.glyphs.find(cp);
    if (it != font.glyphs.end() && it->second.valid) return it->second;

    auto fallbackIt = font.glyphs.find(font.fallbackCodepoint);
    if (fallbackIt != font.glyphs.end()) return fallbackIt->second;

    static const GlyphMetrics kInvalid{};
    return kInvalid;
}

bool SupportsLabelSubset(const TextLabel& label) {
    return label.wrapMode == TextWrapMode::None &&
           label.horizontalAlign == TextHAlign::Left &&
           label.verticalAlign == TextVAlign::Top &&
           label.overflowMode == TextOverflowMode::Clip;
}

} // namespace

std::shared_ptr<ASCIIgL::Mesh> TextLabelMeshBuilder::BuildMesh(const BitmapFont& font,
                                                               std::string_view text,
                                                               float scale,
                                                               const TextLabel& label,
                                                               glm::vec2 labelSizePx) {
    if (!font.textureArray || text.empty() || scale <= 0.0f) return nullptr;
    if (!SupportsLabelSubset(label)) {
        ASCIIgL::Logger::Warning("TextLabelMeshBuilder currently supports only NoWrap + Left/Top + Clip style.");
        return nullptr;
    }
    if (labelSizePx.x <= 0.0f || labelSizePx.y <= 0.0f) return nullptr;

    using V = ASCIIgL::VertStructs::PosUVLayer;
    std::vector<V> vertices;
    std::vector<int> indices;
    vertices.reserve(text.size() * 4);
    indices.reserve(text.size() * 6);

    glm::vec2 pen(0.0f, 0.0f);
    const float lineHeight = std::max(font.lineHeight, static_cast<float>(font.cellHeight))
                           * label.lineHeightMultiplier * scale;

    for (char ch : text) {
        if (ch == '\n') {
            pen.x = 0.0f;
            pen.y += lineHeight;
            if (pen.y >= labelSizePx.y) break;
            continue;
        }

        const auto& glyph = ResolveGlyph(font, static_cast<unsigned char>(ch));
        if (!glyph.valid || glyph.layer < 0) continue;

        const float glyphW = glyph.width * scale;
        const float glyphH = glyph.height * scale;
        const bool isSpace = (ch == ' ');
        const float spacing = (isSpace ? (font.wordSpacing + label.wordSpacing)
                                       : (font.letterSpacing + label.letterSpacing)) * scale;

        // Clip overflow mode: stop emitting once glyph would exceed bounds.
        if (pen.x + glyphW > labelSizePx.x) break;
        if (pen.y + glyphH > labelSizePx.y) break;

        const float x0 = pen.x;
        const float y0 = pen.y;
        const float x1 = x0 + glyphW;
        const float y1 = y0 + glyphH;
        const float layer = static_cast<float>(glyph.layer);
        // D3D11 V=0 at texture top; y0/y1 are screen Y-down (top/bottom).
        V v0{}; v0.SetXYZ({x0, y0, 0.0f}); v0.SetUV({0.0f, 0.0f}); v0.SetLayer(layer);
        V v1{}; v1.SetXYZ({x0, y1, 0.0f}); v1.SetUV({0.0f, 1.0f}); v1.SetLayer(layer);
        V v2{}; v2.SetXYZ({x1, y1, 0.0f}); v2.SetUV({1.0f, 1.0f}); v2.SetLayer(layer);
        V v3{}; v3.SetXYZ({x1, y0, 0.0f}); v3.SetUV({1.0f, 0.0f}); v3.SetLayer(layer);

        const int base = static_cast<int>(vertices.size());
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);

        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);

        pen.x += (glyph.advance * scale) + spacing;
    }

    if (vertices.empty()) return nullptr;

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices),
        font.textureArray.get()
    );
}

} // namespace gui::text
