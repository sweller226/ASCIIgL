#include <ASCIICraft/gui/Screen.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

namespace ASCIICraft::gui {

void Screen::OnUpdate(float dt) {
    Panel::Update(dt);
}

void Screen::OnDraw(::ecs::systems::RenderSystem& renderSystem) const {
    Panel::Draw(renderSystem);
}

} // namespace ASCIICraft::gui
