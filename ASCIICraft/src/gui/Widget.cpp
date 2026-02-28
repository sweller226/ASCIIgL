#include <ASCIICraft/gui/Widget.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

namespace gui {

void Widget::Layout(glm::vec2 screenSize, const glm::vec2* parentTopLeft) {
    if (parentTopLeft) {
        // Child: top-left = parent top-left + offset - pivot*size (offset is in parent space)
        screenPosition = *parentTopLeft + offset - pivot * size;
    } else {
        // Root: anchor point on screen = anchor * screenSize; widget pivot placed there, then offset
        screenPosition = anchor * screenSize - pivot * size + offset;
    }
}

bool Widget::ContainsPoint(glm::vec2 point) const {
    if (!visible || !enabled) return false;
    // screenPosition is always top-left
    return point.x >= screenPosition.x && point.x < screenPosition.x + size.x &&
           point.y >= screenPosition.y && point.y < screenPosition.y + size.y;
}

void Widget::Draw(::ecs::systems::RenderSystem& renderSystem) const {
    (void)renderSystem;
}

Widget* Widget::HitTest(glm::vec2 point) {
    return ContainsPoint(point) ? this : nullptr;
}

const Widget* Widget::HitTest(glm::vec2 point) const {
    return ContainsPoint(point) ? this : nullptr;
}

} // namespace gui
