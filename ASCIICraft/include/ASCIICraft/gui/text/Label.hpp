#pragma once

#include <ASCIICraft/gui/Widget.hpp>
#include <ASCIICraft/gui/text/BitmapFont.hpp>
#include <ASCIICraft/gui/text/TextLabel.hpp>

#include <memory>
#include <string>

namespace ASCIIgL { class Mesh; }

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

    TextLabel& Style() { MarkDirty(); return m_style; }
    const TextLabel& Style() const { return m_style; }
    void SetStyle(const TextLabel& style);

    void MarkDirty() const { m_meshDirty = true; }

    void Draw(gui::GUIRenderer& renderer) const override;

private:
    void RebuildMeshIfNeeded() const;

    const BitmapFont* m_font = nullptr; // non-owning; managed by caller/resource layer
    std::string m_text;
    TextLabel m_style{};

    mutable bool m_meshDirty = true;
    mutable std::shared_ptr<ASCIIgL::Mesh> m_mesh;
};

} // namespace gui::text
