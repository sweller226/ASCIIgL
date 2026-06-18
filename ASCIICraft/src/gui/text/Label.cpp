#include <ASCIICraft/gui/text/Label.hpp>

#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/gui/text/TextLabelMeshBuilder.hpp>

#include <utility>

namespace gui::text {

Label::Label(const BitmapFont* font, std::string text)
    : m_font(font)
    , m_text(std::move(text)) {}

void Label::SetFont(const BitmapFont* font) {
    m_font = font;
    m_meshDirty = true;
}

void Label::SetText(std::string text) {
    if (m_text == text) return;
    m_text = std::move(text);
    m_meshDirty = true;
}

void Label::SetStyle(const TextLabelStyle& style) {
    m_style = style;
    m_meshDirty = true;
}

void Label::Draw(gui::GUIRenderer& renderer) const {
    if (!visible || !enabled) return;

    RebuildMeshIfNeeded();
    if (!m_textMesh) return;

    renderer.RenderTextMesh(screenPosition, layer, m_textMesh.mesh);
}

void Label::RebuildMeshIfNeeded() const {
    if (!m_meshDirty) return;
    m_meshDirty = false;

    if (!m_font || m_text.empty()) {
        m_textMesh = {};
        return;
    }

    TextLabelStyle style = m_style;
    if (style.boundsPx.x <= 0.0f || style.boundsPx.y <= 0.0f) {
        style.boundsPx = size;
    }
    if (style.boundsPx.x <= 0.0f || style.boundsPx.y <= 0.0f) {
        m_textMesh = {};
        return;
    }

    m_textMesh = TextLabelMeshBuilder::Build(*m_font, m_text, style.scale, style);
}

} // namespace gui::text
