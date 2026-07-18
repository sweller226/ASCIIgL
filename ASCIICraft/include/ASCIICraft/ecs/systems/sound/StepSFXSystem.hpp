#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIIgL/util/EventBus.hpp>

namespace blockstate {
class BlockStateRegistry;
} // namespace blockstate

namespace sound {
class SoundRegistry;
} // namespace sound

namespace util {
class RNG;
} // namespace util

class World;

namespace ecs::systems {

class StepSFXSystem : public ISystem {
public:
    StepSFXSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);

    void Update() override;

private:
    void UpdateStepSounds(float deltaTime);

    bool TryEmitStepSound(
        entt::entity ent,
        const glm::vec3& bodyCenter,
        const glm::vec3& halfExtents,
        const World& world,
        const blockstate::BlockStateRegistry& bsr,
        const sound::SoundRegistry& soundRegistry,
        util::RNG& rng
    ) const;

    entt::registry&    m_registry;
    ASCIIgL::EventBus& m_eventBus;
};

} // namespace ecs::systems
