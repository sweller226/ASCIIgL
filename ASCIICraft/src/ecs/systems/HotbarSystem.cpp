#include <ASCIICraft/ecs/systems/HotbarSystem.hpp>

#include <string>

#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIIgL/engine/InputManager.hpp>

namespace ecs::systems {

HotbarSystem::HotbarSystem(entt::registry& registry)
    : registry(registry) {}

void HotbarSystem::Update() {
    using namespace ecs::components;

    auto view = registry.view<PlayerTag, HotbarSelection>();
    if (view.size_hint() == 0) {
        return;
    }

    const entt::entity player = *view.begin();
    auto& selection = view.get<HotbarSelection>(player);

    auto& input = ASCIIgL::InputManager::GetInst();

    for (int i = 0; i < HotbarSelection::kSlotCount; ++i) {
        const std::string action = "hotbar_" + std::to_string(i + 1);
        if (input.IsActionPressed(action)) {
            selection.selectedSlot = i;
        }
    }

    if (input.IsActionPressed("hotbar_0")) {
        selection.selectedSlot = HotbarSelection::kSlotCount - 1;
    }

    selection.clamp();
}

} // namespace ecs::systems
