#include <ASCIICraft/gui/Panel.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

namespace gui {

void Panel::AddChild(std::unique_ptr<Widget> child) {
    if (child) {
        child->SetParent(this);
        children.push_back(std::move(child));
    }
}

void Panel::Layout(glm::vec2 screenSize, const glm::vec2* parentTopLeft) {
    Widget::Layout(screenSize, parentTopLeft);
    for (auto& c : children)
        c->Layout(screenSize, &screenPosition);  // pass this panel's top-left to children
}

Widget* Panel::HitTest(glm::vec2 point) {
    if (!ContainsPoint(point)) return nullptr;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (Widget* w = (*it)->HitTest(point))
            return w;
    }
    return this;
}

const Widget* Panel::HitTest(glm::vec2 point) const {
    if (!ContainsPoint(point)) return nullptr;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (const Widget* w = (*it)->HitTest(point))
            return w;
    }
    return this;
}

void Panel::Update(float dt) {
    Widget::Update(dt);
    for (auto& c : children)
        c->Update(dt);
}

void Panel::Draw(::ecs::systems::RenderSystem& renderSystem) const {
    if (!visible) return;
    if (backgroundMesh && backgroundMaterial)
        renderSystem.AddGuiItem(screenPosition, size, layer, backgroundMesh, backgroundMaterial);
    for (const auto& c : children)
        c->Draw(renderSystem);
}

} // namespace gui
