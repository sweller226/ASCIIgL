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

bool SupportsLabelSubset(const TextLabelStyle& style) {
    return style.wrapMode == TextWrapMode::None &&
           style.overflowMode == TextOverflowMode::Clip;
}

glm::vec2 ComputeAlignmentOffset(glm::vec2 contentSizePx,
                                 glm::vec2 boundsPx,
                                 TextHAlign horizontalAlign,
                                 TextVAlign verticalAlign) {
    glm::vec2 offset{0.0f, 0.0f};

    switch (horizontalAlign) {
    case TextHAlign::Right:
        offset.x = boundsPx.x - contentSizePx.x;
        break;
    case TextHAlign::Center:
        offset.x = (boundsPx.x - contentSizePx.x) * 0.5f;
        break;
    default:
        break;
    }

    switch (verticalAlign) {
    case TextVAlign::Bottom:
        offset.y = boundsPx.y - contentSizePx.y;
        break;
    case TextVAlign::Middle:
        offset.y = (boundsPx.y - contentSizePx.y) * 0.5f;
        break;
    default:
        break;
    }

    return offset;
}

} // namespace

TextLabelMesh TextLabelMeshBuilder::Build(const BitmapFont& font,
                                          std::string_view text,
                                          float scale,
                                          const TextLabelStyle& style) {
    if (!font.textureArray || text.empty() || scale <= 0.0f) return {};
    if (!SupportsLabelSubset(style)) {
        ASCIIgL::Logger::Warning("TextLabelMeshBuilder currently supports only NoWrap + Clip style.");
        return {};
    }
    const glm::vec2 boundsPx = style.boundsPx;
    if (boundsPx.x <= 0.0f || boundsPx.y <= 0.0f) return {};

    using V = ASCIIgL::VertStructs::PosUVLayer;
    std::vector<V> vertices;
    std::vector<int> indices;
    vertices.reserve(text.size() * 4);
    indices.reserve(text.size() * 6);

    glm::vec2 pen(0.0f, 0.0f);
    float maxRight = 0.0f;
    float maxBottom = 0.0f;
    const float lineHeight = font.lineHeight * style.lineHeightMultiplier * scale;

    for (char ch : text) {
        if (ch == '\n') {
            pen.x = 0.0f;
            pen.y += lineHeight;
            if (pen.y >= boundsPx.y) break;
            continue;
        }

        const auto& glyph = ResolveGlyph(font, static_cast<unsigned char>(ch));
        if (!glyph.valid || glyph.layer < 0) continue;

        const float glyphW = glyph.width * scale;
        const float glyphH = glyph.height * scale;
        const bool isSpace = (ch == ' ');
        const float spacing = (isSpace ? (font.wordSpacing + style.wordSpacing)
                                       : (font.letterSpacing + style.letterSpacing)) * scale;

        // Clip overflow mode: stop emitting once glyph would exceed bounds.
        if (pen.x + glyphW > boundsPx.x) break;
        if (pen.y + glyphH > boundsPx.y) break;

        const float x0 = pen.x;
        const float y0 = pen.y;
        const float x1 = x0 + glyphW;
        const float y1 = y0 + glyphH;
        maxRight = std::max(maxRight, x1);
        maxBottom = std::max(maxBottom, y1);
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

    if (vertices.empty()) return {};

    const glm::vec2 contentSizePx{maxRight, maxBottom};
    const glm::vec2 alignOffset = ComputeAlignmentOffset(
        contentSizePx, boundsPx, style.horizontalAlign, style.verticalAlign
    );
    if (alignOffset.x != 0.0f || alignOffset.y != 0.0f) {
        for (auto& vertex : vertices) {
            glm::vec3 pos = vertex.GetXYZ();
            pos.x += alignOffset.x;
            pos.y += alignOffset.y;
            vertex.SetXYZ(pos);
        }
    }

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    TextLabelMesh result;
    result.contentSizePx = contentSizePx;
    result.mesh = std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices),
        font.textureArray.get()
    );
    return result;
}

} // namespace gui::text
