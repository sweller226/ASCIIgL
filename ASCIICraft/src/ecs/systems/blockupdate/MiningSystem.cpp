#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/ecs/components/BlockTarget.hpp>
#include <ASCIICraft/ecs/components/HandSwing.hpp>
#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemProperties.hpp>
#include <ASCIICraft/ecs/components/MiningProgress.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>
#include <ASCIICraft/events/BlockHitEvent.hpp>
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIICraft/events/SoundEvents.hpp>
#include <ASCIICraft/sound/BlockSoundMap.hpp>
#include <ASCIICraft/sound/SoundRegistry.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

namespace ecs::systems {

MiningSystem::MiningSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus)
    : m_registry(registry)
    , m_eventBus(eventBus)
{}

void MiningSystem::Update() {
    entt::entity playerEnt = components::GetPlayerEntity(m_registry);
    if (playerEnt == entt::null) return;

    const auto* pmode = m_registry.try_get<components::PlayerMode>(playerEnt);
    const GameMode mode = pmode ? pmode->gamemode : GameMode::Survival;

    switch (mode) {
        case GameMode::Creative:
        case GameMode::Spectator:
            CreativeBreakEvents(playerEnt);
            break;
        case GameMode::Survival:
            SurvivalMining(playerEnt, ASCIIgL::FPSClock::GetInst().GetDeltaTime());
            break;
    }
}

void MiningSystem::CreativeBreakEvents(entt::entity playerEnt) {
    // Keep survival progress cleared while in creative.
    if (auto* mining = m_registry.try_get<components::MiningProgress>(playerEnt)) {
        mining->active = false;
        mining->progress = 0.0f;
    }

    for ([[maybe_unused]] const auto& e : m_eventBus.view<events::PrimaryActionPressedEvent>()) {
        const auto* target = m_registry.try_get<components::BlockTarget>(playerEnt);
        if (!target || !target->active) break;

        events::BreakBlockEvent breakEvent;
        breakEvent.stateId = target->stateId;
        breakEvent.position = target->blockPos;
        breakEvent.harvested = true;
        m_eventBus.emit(breakEvent);
        EmitBreakSound(playerEnt, target->stateId);
        if (auto* swing = m_registry.try_get<components::HandSwing>(playerEnt)) {
            swing->Trigger();
        }
        break;
    }
}

void MiningSystem::SurvivalMining(entt::entity playerEnt, float dt) {
    auto* mining = m_registry.try_get<components::MiningProgress>(playerEnt);
    if (!mining) return;

    if (mining->breakCooldown > 0.0f) {
        mining->breakCooldown -= dt;
    }

    const bool held = !m_eventBus.view<events::PrimaryActionHeldEvent>().empty();
    const auto* target = m_registry.try_get<components::BlockTarget>(playerEnt);

    if (!held || !target || !target->active) {
        mining->active = false;
        mining->progress = 0.0f;
        m_hitEffectTimer = 0.0f;
        return;
    }

    // Target switched (or the block changed under us): discard partial progress.
    if (!mining->active
        || mining->blockPos != target->blockPos
        || mining->stateId != target->stateId) {
        mining->active = true;
        mining->blockPos = target->blockPos;
        mining->stateId = target->stateId;
        mining->progress = 0.0f;
    }

    if (mining->breakCooldown > 0.0f) {
        return;
    }

    const auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
    if (!bsr || !bsr->IsValidState(mining->stateId)) {
        mining->active = false;
        mining->progress = 0.0f;
        return;
    }

    const auto& type = bsr->GetType(bsr->GetTypeIdFromState(mining->stateId));

    // Unbreakable (hardness < 0): show no progress, never break.
    if (type.hardness < 0.0f) {
        mining->progress = 0.0f;
        return;
    }

    // Keep the arm swinging for as long as the block is being mined.
    if (auto* swing = m_registry.try_get<components::HandSwing>(playerEnt)) {
        swing->Trigger();
    }

    const components::ToolProperty* tool = GetHeldTool(playerEnt);
    const bool toolMatches = tool
        && tool->toolClass != ToolClass::None
        && tool->toolClass == type.preferredTool;
    const float speed = toolMatches ? tool->miningSpeed : 1.0f;
    const bool canHarvest = type.requiredHarvestLevel == 0
        || (toolMatches && tool->harvestLevel >= type.requiredHarvestLevel);

    if (type.hardness == 0.0f) {
        mining->progress = 1.0f;
    } else {
        // Vanilla: damage per tick = speed / hardness / (30 or 100), 20 ticks/s.
        const float divisor = canHarvest ? 30.0f : 100.0f;
        mining->progress += (speed / type.hardness / divisor) * 20.0f * dt;
        EmitHitEffects(playerEnt, *mining, dt);
    }

    if (mining->progress >= 1.0f) {
        EmitBreak(*mining, canHarvest);
        EmitBreakSound(playerEnt, mining->stateId);
        mining->active = false;
        mining->progress = 0.0f;
        mining->breakCooldown = kBreakCooldownSeconds;
    }
}

const components::ToolProperty* MiningSystem::GetHeldTool(entt::entity playerEnt) const {
    const auto* inventory = m_registry.try_get<components::Inventory>(playerEnt);
    const auto* hotbar = m_registry.try_get<components::HotbarSelection>(playerEnt);
    if (!inventory || !hotbar) return nullptr;
    if (hotbar->selectedSlot < 0 || hotbar->selectedSlot >= inventory->capacity) return nullptr;

    const components::ItemStack& stack = inventory->slots[hotbar->selectedSlot];
    if (stack.isEmpty()) return nullptr;

    const auto* itemRegistry = m_registry.ctx().find<data::ItemRegistry>();
    if (!itemRegistry) return nullptr;

    const entt::entity itemDef = itemRegistry->Resolve(stack.itemId);
    if (itemDef == entt::null) return nullptr;

    return m_registry.try_get<components::ToolProperty>(itemDef);
}

void MiningSystem::EmitHitEffects(
    entt::entity playerEnt,
    const components::MiningProgress& mining,
    float dt
) {
    m_hitEffectTimer -= dt;
    if (m_hitEffectTimer > 0.0f) return;
    m_hitEffectTimer = kHitEffectIntervalSeconds;

    // Approximate the mined face as the block face pointing toward the camera.
    FaceDir face = FaceDir::Top;
    if (const auto* cam = m_registry.try_get<components::PlayerCamera>(playerEnt)) {
        const glm::vec3 center(
            static_cast<float>(mining.blockPos.x) + 0.5f,
            static_cast<float>(mining.blockPos.y) + 0.5f,
            static_cast<float>(mining.blockPos.z) + 0.5f
        );
        face = FaceDirFromOutwardNormal(cam->camera.pos - center);
    }

    events::BlockHitEvent hitEvent;
    hitEvent.position = mining.blockPos;
    hitEvent.stateId = mining.stateId;
    hitEvent.face = face;
    m_eventBus.emit(hitEvent);

    // Hit loop: vanilla reuses the block's step sound, quieter and lower-pitched.
    const auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
    auto* soundMap = m_registry.ctx().find<sound::BlockSoundMap>();
    const auto* soundRegistry = m_registry.ctx().find<sound::SoundRegistry>();
    if (bsr && soundMap && soundRegistry) {
        const uint16_t typeId = bsr->GetTypeIdFromStateOr(mining.stateId, 0);
        const std::string soundId = soundMap->ResolveStepSoundId(typeId, *bsr);
        if (soundRegistry->Has(soundId)) {
            m_eventBus.emit(events::PlaySoundEvent{soundId, playerEnt, 0.25f, 0.5f});
        }
    }
}

void MiningSystem::EmitBreakSound(entt::entity playerEnt, uint32_t stateId) {
    const auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
    auto* soundMap = m_registry.ctx().find<sound::BlockSoundMap>();
    const auto* soundRegistry = m_registry.ctx().find<sound::SoundRegistry>();
    if (!bsr || !soundMap || !soundRegistry) return;

    const uint16_t typeId = bsr->GetTypeIdFromStateOr(stateId, 0);
    // Vanilla plays the dedicated dig/break sound on destroy (step sounds are
    // only used for the hit loop while mining).
    std::string soundId = soundMap->ResolveDigSoundId(typeId, *bsr);
    if (!soundRegistry->Has(soundId)) {
        soundId = soundMap->ResolveStepSoundId(typeId, *bsr);
        if (!soundRegistry->Has(soundId)) return;
    }

    m_eventBus.emit(events::PlaySoundEvent{soundId, playerEnt, 1.0f, 0.8f});
}

void MiningSystem::EmitBreak(const components::MiningProgress& mining, bool harvested) {
    events::BreakBlockEvent breakEvent;
    breakEvent.stateId = mining.stateId;
    breakEvent.position = mining.blockPos;
    breakEvent.harvested = harvested;
    m_eventBus.emit(breakEvent);
}

} // namespace ecs::systems
