#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

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
    void RebuildStepSoundMap(const blockstate::BlockStateRegistry& bsr);
    void UpdateStepSounds(float deltaTime);

    std::string ResolveStepSoundId(uint16_t typeId, const blockstate::BlockStateRegistry& bsr) const;

    bool TryEmitStepSound(
        entt::entity ent,
        const glm::vec3& bodyCenter,
        const glm::vec3& halfExtents,
        const World& world,
        const blockstate::BlockStateRegistry& bsr,
        const sound::SoundRegistry& soundRegistry,
        util::RNG& rng
    ) const;

    void MapType(const blockstate::BlockStateRegistry& bsr, const char* typeName, const char* soundId);

    static constexpr float MIN_STEP_SPEED       = 0.15f;
    static constexpr float STEP_COOLDOWN        = 0.10f;
    static constexpr float WALK_STEP_DISTANCE   = 1.6f;
    static constexpr float RUN_STEP_DISTANCE    = 1.4f;
    static constexpr float SNEAK_STEP_DISTANCE  = 2.0f;

    static constexpr const char* DEFAULT_STEP_SOUND_ID = "step.stone";

    entt::registry&    m_registry;
    ASCIIgL::EventBus& m_eventBus;

    std::unordered_map<uint16_t, std::string> m_stepSoundByTypeId;
    bool m_mapBuilt = false;
};

} // namespace ecs::systems
