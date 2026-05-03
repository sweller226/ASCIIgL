// ecs/systems/LifetimeSystem.cpp
#include <ASCIICraft/ecs/systems/LifetimeSystem.hpp>

#include <ASCIICraft/ecs/components/Lifetime.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

namespace ecs::systems {

LifetimeSystem::LifetimeSystem(entt::registry& registry)
    : m_registry(registry) {}

void LifetimeSystem::Update() {
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

    auto view = m_registry.view<components::Lifetime>();
    for (auto [ent, lifetime] : view.each()) {
        lifetime.ageSeconds += dt;

        if (lifetime.maxLifetimeSeconds > 0.0f && lifetime.ageSeconds >= lifetime.maxLifetimeSeconds)
            lifetime.shouldDespawn = true;
    }
}

} // namespace ecs::systems