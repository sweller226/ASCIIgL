#include <ASCIICraft/ecs/systems/sound/StepSFXSystem.hpp>

#include <glm/glm.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/ecs/StepTiming.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/StepSoundState.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/ViewBobbing.hpp>
#include <ASCIICraft/events/SoundEvents.hpp>
#include <ASCIICraft/sound/SoundRegistry.hpp>
#include <ASCIICraft/util/RNG.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/query/BlockQueries.hpp>
#include <ASCIICraft/world/query/VoxelOverlap.hpp>

namespace ecs::systems {

namespace {

void ConsumeStep(components::StepSoundState& stepState, const glm::vec3& bodyCenter, float cooldown) {
    stepState.distanceAccum = 0.0f;
    stepState.cooldown = cooldown;
    stepState.lastPosition = bodyCenter;
}

float GetStepVolume(const components::PlayerController* ctrl) {
    return (ctrl && ctrl->movementState == MovementState::Sneaking) ? 0.4f : 1.0f;
}

} // namespace

StepSFXSystem::StepSFXSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus)
    : m_registry(registry)
    , m_eventBus(eventBus)
{}

void StepSFXSystem::Update() {
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    UpdateStepSounds(dt);
}

void StepSFXSystem::MapType(
    const blockstate::BlockStateRegistry& bsr,
    const char* typeName,
    const char* soundId
) {
    const uint16_t typeId = bsr.GetTypeId(typeName);
    m_stepSoundByTypeId[typeId] = soundId;
}

void StepSFXSystem::RebuildStepSoundMap(const blockstate::BlockStateRegistry& bsr) {
    m_stepSoundByTypeId.clear();

    MapType(bsr, "minecraft:grass", "step.grass");
    MapType(bsr, "minecraft:dirt", "step.gravel");
    MapType(bsr, "minecraft:tall_grass", "step.grass");
    MapType(bsr, "minecraft:fern", "step.grass");
    MapType(bsr, "minecraft:dandelion", "step.grass");
    MapType(bsr, "minecraft:poppy", "step.grass");

    MapType(bsr, "minecraft:stone", "step.stone");
    MapType(bsr, "minecraft:cobblestone", "step.stone");
    MapType(bsr, "minecraft:bedrock", "step.stone");
    MapType(bsr, "minecraft:brick_block", "step.stone");
    MapType(bsr, "minecraft:stone_stairs", "step.stone");
    MapType(bsr, "minecraft:cobblestone_slab", "step.stone");
    MapType(bsr, "minecraft:glass", "step.stone");

    MapType(bsr, "minecraft:oak_log", "step.wood");
    MapType(bsr, "minecraft:oak_planks", "step.wood");
    MapType(bsr, "minecraft:oak_slab", "step.wood");
    MapType(bsr, "minecraft:fence", "step.wood");
    MapType(bsr, "minecraft:crafting_table", "step.wood");
    MapType(bsr, "minecraft:bookshelf", "step.wood");
    MapType(bsr, "minecraft:furnace", "step.wood");

    MapType(bsr, "minecraft:oak_leaves", "step.grass");

    m_mapBuilt = true;
}

std::string StepSFXSystem::ResolveStepSoundId(
    uint16_t typeId,
    const blockstate::BlockStateRegistry& bsr
) const {
    const auto it = m_stepSoundByTypeId.find(typeId);
    if (it != m_stepSoundByTypeId.end()) {
        return it->second;
    }

    if (typeId < bsr.GetTotalTypeCount()) {
        const std::string& name = bsr.GetType(typeId).name;
        if (name.find("grass") != std::string::npos || name.find("leaves") != std::string::npos) {
            return "step.grass";
        }
        if (name.find("dirt") != std::string::npos || name.find("gravel") != std::string::npos) {
            return "step.gravel";
        }
        if (name.find("log") != std::string::npos || name.find("planks") != std::string::npos ||
            name.find("wood") != std::string::npos || name.find("fence") != std::string::npos) {
            return "step.wood";
        }
        if (name.find("sand") != std::string::npos) {
            return "step.sand";
        }
        if (name.find("snow") != std::string::npos) {
            return "step.snow";
        }
        if (name.find("wool") != std::string::npos || name.find("carpet") != std::string::npos) {
            return "step.cloth";
        }
    }

    return DEFAULT_STEP_SOUND_ID;
}

bool StepSFXSystem::TryEmitStepSound(
    entt::entity ent,
    const glm::vec3& bodyCenter,
    const glm::vec3& halfExtents,
    const World& world,
    const blockstate::BlockStateRegistry& bsr,
    const sound::SoundRegistry& soundRegistry,
    util::RNG& rng
) const {
    const worldquery::VoxelOverlapHit floorBlock = worldquery::SampleFloorBlock(
        world,
        bodyCenter,
        halfExtents
    );
    if (blockquery::IsAir(floorBlock.stateId)) {
        return false;
    }

    const uint16_t typeId = bsr.GetTypeIdFromStateOr(floorBlock.stateId, 0);
    const std::string soundId = ResolveStepSoundId(typeId, bsr);
    if (!soundRegistry.Has(soundId)) {
        ASCIIgL::Logger::Warningf("[StepSFXSystem] Unregistered step sound id: %s", soundId.c_str());
        return false;
    }

    m_eventBus.emit(events::PlaySoundEvent{
        soundId,
        ent,
        GetStepVolume(m_registry.try_get<components::PlayerController>(ent)),
        rng.NextFloat(0.9f, 1.1f),
    });
    return true;
}

void StepSFXSystem::UpdateStepSounds(float deltaTime) {
    const World* world = GetWorldPtr(m_registry);
    const auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
    if (!world || !bsr) {
        return;
    }

    if (!m_mapBuilt) {
        RebuildStepSoundMap(*bsr);
    }

    const auto* soundRegistry = m_registry.ctx().find<sound::SoundRegistry>();
    if (!soundRegistry) {
        return;
    }

    static util::RNG rng;

    auto view = m_registry.view<
        components::Transform,
        components::Collider,
        components::GroundPhysics,
        components::Velocity,
        components::StepSoundState
    >();

    for (auto [ent, transform, collider, ground, vel, stepState] : view.each()) {
        if (collider.disabled) {
            stepState.wasOnGround = false;
            continue;
        }

        const auto* flying = m_registry.try_get<components::FlyingPhysics>(ent);
        if (flying && flying->enabled) {
            stepState.wasOnGround = false;
            continue;
        }

        const auto* ctrl = m_registry.try_get<components::PlayerController>(ent);
        const glm::vec3 bodyCenter = transform.position + collider.localOffset;
        const float horizSpeed = glm::length(glm::vec2(vel.linear.x, vel.linear.z));
        const bool landedThisFrame = ground.onGround && !stepState.wasOnGround;
        stepState.wasOnGround = ground.onGround;
        stepState.cooldown = std::max(0.0f, stepState.cooldown - deltaTime);

        if (!ground.onGround) {
            stepState.lastPosition = bodyCenter;
            continue;
        }

        if (auto* bob = m_registry.try_get<components::ViewBobbing>(ent)) {
            if (landedThisFrame && stepState.cooldown <= 0.0f && horizSpeed >= StepTiming::MIN_STEP_SPEED) {
                if (TryEmitStepSound(
                    ent,
                    bodyCenter,
                    collider.halfExtents,
                    *world,
                    *bsr,
                    *soundRegistry,
                    rng
                )) {
                    ConsumeStep(stepState, bodyCenter, StepTiming::STEP_COOLDOWN);
                    bob->nextStepDistance = static_cast<int>(bob->distanceWalkedOnStepModified) + 1;
                }
            }

            if (stepState.cooldown > 0.0f) {
                continue;
            }

            if (horizSpeed < StepTiming::MIN_STEP_SPEED) {
                stepState.lastPosition = bodyCenter;
                continue;
            }

            if (bob->distanceWalkedOnStepModified <= static_cast<float>(bob->nextStepDistance)) {
                continue;
            }

            if (!TryEmitStepSound(
                ent,
                bodyCenter,
                collider.halfExtents,
                *world,
                *bsr,
                *soundRegistry,
                rng
            )) {
                continue;
            }

            bob->nextStepDistance = static_cast<int>(bob->distanceWalkedOnStepModified) + 1;
            ConsumeStep(stepState, bodyCenter, StepTiming::STEP_COOLDOWN);
            continue;
        }

        if (landedThisFrame && stepState.cooldown <= 0.0f && horizSpeed >= StepTiming::MIN_STEP_SPEED) {
            if (TryEmitStepSound(
                ent,
                bodyCenter,
                collider.halfExtents,
                *world,
                *bsr,
                *soundRegistry,
                rng
            )) {
                ConsumeStep(stepState, bodyCenter, StepTiming::STEP_COOLDOWN);
                continue;
            }
        }

        if (stepState.cooldown > 0.0f) {
            continue;
        }

        if (horizSpeed < StepTiming::MIN_STEP_SPEED) {
            stepState.distanceAccum = 0.0f;
            stepState.lastPosition = bodyCenter;
            continue;
        }

        const glm::vec2 delta(bodyCenter.x - stepState.lastPosition.x, bodyCenter.z - stepState.lastPosition.z);
        stepState.distanceAccum += glm::length(delta);
        stepState.lastPosition = bodyCenter;

        const float stepDistance = StepTiming::GetStepDistance(ctrl);

        if (stepState.distanceAccum < stepDistance) {
            continue;
        }

        if (!TryEmitStepSound(
            ent,
            bodyCenter,
            collider.halfExtents,
            *world,
            *bsr,
            *soundRegistry,
            rng
        )) {
            stepState.distanceAccum = 0.0f;
            continue;
        }

        ConsumeStep(stepState, bodyCenter, StepTiming::STEP_COOLDOWN);
    }
}

} // namespace ecs::systems
