#pragma once

#include <ASCIICraft/gui/Widget.hpp>
#include <ASCIICraft/gui/text/BitmapFont.hpp>
#include <ASCIICraft/gui/text/TextLabelStyle.hpp>
#include <ASCIICraft/gui/text/TextLabelMesh.hpp>

#include <memory>
#include <string>

namespace gui::text {

/// GUI widget that renders bitmap-font text via TextLabelMeshBuilder.
class Label : public gui::Widget {
public:
    Label() = default;
    Label(const BitmapFont* font, std::string text);

    void SetFont(const BitmapFont* font);
    const BitmapFont* GetFont() const { return m_font; }

    void SetText(std::string text);
    const std::string& GetText() const { return m_text; }

    TextLabelStyle& Style() { MarkDirty(); return m_style; }
    const TextLabelStyle& Style() const { return m_style; }
    void SetStyle(const TextLabelStyle& style);

    const glm::vec2& ContentSizePx() const { return m_textMesh.contentSizePx; }

    void MarkDirty() const { m_meshDirty = true; }

    void Draw(gui::GUIRenderer& renderer) const override;

private:
    void RebuildMeshIfNeeded() const;

    const BitmapFont* m_font = nullptr; // non-owning; managed by caller/resource layer
    std::string m_text;
    TextLabelStyle m_style{};

    mutable bool m_meshDirty = true;
    mutable TextLabelMesh m_textMesh{};
};

} // namespace gui::text
