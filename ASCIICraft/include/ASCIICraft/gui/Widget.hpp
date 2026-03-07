#pragma once

#include <glm/vec2.hpp>
#include <memory>
#include <vector>

namespace ASCIIgL { class Texture; }
namespace ecs::systems { class RenderSystem; }

namespace gui {

/// Base for all GUI widgets. Layout uses anchor/pivot/offset/size; optional parent for relative layout.
class Widget {
public:
    virtual ~Widget() = default;

    glm::vec2 anchor{0.5f, 0.5f};   // Normalized (0-1): where on parent/screen the pivot is placed
    glm::vec2 pivot{0.0f, 0.0f};    // Pivot within element (0-1). (0,0)=top-left, (0.5,0.5)=center
    glm::vec2 offset{0.0f, 0.0f};   // Pixel offset from anchor point
    glm::vec2 size{100.0f, 100.0f};
    int layer = 0;
    bool visible = true;
    bool enabled = true;

    /// Computed after Layout(). Always the widget's top-left in screen pixels.
    glm::vec2 screenPosition{0.0f, 0.0f};

    /// Layout this widget. If parentTopLeft is null, use full screen (anchor * size); else position = parentTopLeft + offset - pivot*size.
    virtual void Layout(glm::vec2 screenSize, const glm::vec2* parentTopLeft = nullptr);

    bool ContainsPoint(glm::vec2 point) const;

    /// Topmost widget at point (for hit-test). Base returns this if ContainsPoint else nullptr.
    virtual Widget* HitTest(glm::vec2 point);
    virtual const Widget* HitTest(glm::vec2 point) const;

    virtual void Update(float dt) { (void)dt; }
    virtual void Draw(::ecs::systems::RenderSystem& renderSystem) const;

    /// Set parent for layout (used by Panel::AddChild). Protected so only containers set it.
    void SetParent(Widget* p) { parent = p; }

protected:
    Widget* parent = nullptr;
};

} // namespace gui
