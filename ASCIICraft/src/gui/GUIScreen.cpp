#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

namespace gui {

void GUIScreen::OnUpdate(float dt) {
    Panel::Update(dt);
}

void GUIScreen::OnDraw(::ecs::systems::RenderSystem& ecsRenderSystem) const {
    Panel::Draw(ecsRenderSystem);
}

} // namespace gui
