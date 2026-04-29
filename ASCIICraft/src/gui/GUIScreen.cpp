#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

namespace gui {

void GUIScreen::OnUpdate(float dt) {
    Panel::Update(dt);
}

void GUIScreen::OnDraw(::ecs::systems::RenderSystem& renderSystem) const {
    Panel::Draw(renderSystem);
}

} // namespace gui
