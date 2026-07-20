#pragma once

#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <entt/entt.hpp>

namespace ecs::components {
struct BlockTarget;
struct MiningProgress;
struct ToolProperty;
}

namespace ecs::systems {

/// Handles block breaking.
/// - Creative / Spectator: instant break on primary-action press.
/// - Survival: timed mining while primary action is held, using the Minecraft
///   formula (block hardness, tool class match, harvest level gating drops).
class MiningSystem : public ISystem {
public:
    MiningSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);

    void Update() override;

private:
    entt::registry& m_registry;
    ASCIIgL::EventBus& m_eventBus;

    void CreativeBreakEvents(entt::entity playerEnt);
    void SurvivalMining(entt::entity playerEnt, float dt);

    /// ToolProperty of the held hotbar item, or nullptr when not holding a tool.
    const components::ToolProperty* GetHeldTool(entt::entity playerEnt) const;

    void EmitBreak(const components::MiningProgress& mining, bool harvested);

    /// Periodic crack-puff particles + dig sound on the mined face while mining.
    void EmitHitEffects(entt::entity playerEnt, const components::MiningProgress& mining, float dt);

    /// Block break sound (step sound family at lower pitch, like vanilla).
    void EmitBreakSound(entt::entity playerEnt, uint32_t stateId);

    /// Seconds mining is blocked after a survival break (Minecraft's block hit delay).
    static constexpr float kBreakCooldownSeconds = 0.3f;
    /// Interval between crack-puff particle emissions while mining.
    static constexpr float kHitEffectIntervalSeconds = 0.25f;

    float m_hitEffectTimer = 0.0f;
};

}
