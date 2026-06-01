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

void Label::SetStyle(const TextLabel& style) {
    m_style = style;
    m_meshDirty = true;
}

void Label::Draw(gui::GUIRenderer& renderer) const {
    if (!visible || !enabled) return;

    RebuildMeshIfNeeded();
    if (!m_mesh) return;

    renderer.RenderTextMesh(screenPosition, layer, m_mesh);
}

void Label::RebuildMeshIfNeeded() const {
    if (!m_meshDirty) return;
    m_meshDirty = false;

    if (!m_font || m_text.empty()) {
        m_mesh.reset();
        return;
    }

    TextLabel label = m_style;
    glm::vec2 bounds = label.boundsPx;
    if (bounds.x <= 0.0f || bounds.y <= 0.0f) {
        bounds = size;
    }

    m_mesh = TextLabelMeshBuilder::BuildMesh(*m_font, m_text, label.scale, label, bounds);
}

} // namespace gui::text
