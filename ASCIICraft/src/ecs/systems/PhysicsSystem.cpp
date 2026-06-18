// ecs/systems/PhysicsSystem.cpp
#include <ASCIICraft/ecs/systems/PhysicsSystem.hpp>

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/query/VoxelOverlap.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/ViewBobbing.hpp>


namespace ecs::systems {

bool PhysicsSystem::VoxelOverlapProbe::operator()(const glm::vec3 &center) const {
    return worldquery::OverlapsSolidForPhysics(world, bsr, center, halfExtents, colliderDisabled);
}

PhysicsSystem::PhysicsSystem(entt::registry &registry)
  : m_registry(registry)
  , m_accumulator(0.0f)
{}

void PhysicsSystem::Update() {
    const float frameDt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    m_accumulator += frameDt;

    constexpr float maxAccumulator = FixedDt * 5.0f;
    m_accumulator = std::min(m_accumulator, maxAccumulator);

    for (auto [ent, t] : m_registry.view<components::Transform>().each()) {
        t.previousPosition = t.position;
    }

    while (m_accumulator >= FixedDt) {
        Step(FixedDt);
        m_accumulator -= FixedDt;
    }

    const float alpha = m_accumulator / FixedDt;
    for (auto [ent, t] : m_registry.view<components::Transform>().each()) {
        t.renderPosition = glm::mix(t.previousPosition, t.position, alpha);
    }

    for (auto [ent, cam, t] : m_registry.view<components::PlayerCamera, components::Transform>().each()) {
        if (m_registry.all_of<components::ViewBobbing>(ent)) {
            continue;
        }
        cam.camera.setCamPos(t.renderPosition + glm::vec3(0.0f, cam.PLAYER_EYE_HEIGHT, 0.0f));
    }
}

void PhysicsSystem::Step(float fixedDt) {
    const World *world = GetWorldPtr(m_registry);
    if (!world) {
        return;
    }
    IntegrateEntities(fixedDt, *world);
}

void PhysicsSystem::IntegrateEntities(float dt, const World &world) {
    const auto *bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();

    for (auto [ent, t, v] : m_registry.view<components::Transform, components::Velocity>().each()) {
        const auto *gravityComp = m_registry.try_get<components::Gravity>(ent);
        const auto *stepComp = m_registry.try_get<components::StepPhysics>(ent);
        auto *groundComp = m_registry.try_get<components::GroundPhysics>(ent);
        const auto *flyingComp = m_registry.try_get<components::FlyingPhysics>(ent);
        auto *col = m_registry.try_get<components::Collider>(ent);

        const bool canFly = flyingComp && flyingComp->enabled;

        if (gravityComp && !canFly) {
            v.linear += gravityComp->acceleration * dt;
        }

        if (!canFly) {
            v.ApplyDamping(dt);
        }

        v.ClampSpeed();

        if (col && !col->disabled) {
            ResolveAABBAgainstWorld(t, *col, v, dt, world, bsr, stepComp, groundComp);
        } else {
            t.setPosition(t.position + v.linear * dt);
        }
    }
}

void PhysicsSystem::ResolveAABBAgainstWorld(
    components::Transform &t,
    components::Collider &col,
    components::Velocity &vel,
    float dt,
    const World &world,
    const blockstate::BlockStateRegistry *bsr,
    const components::StepPhysics *stepPhysics,
    components::GroundPhysics *groundPhysics
) {
    glm::vec3 pos = t.position + col.localOffset;
    const glm::vec3 half = col.halfExtents;

    const VoxelOverlapProbe overlaps{&world, bsr, half, col.disabled};

    // ===== VERTICAL =====
    const float dy = vel.linear.y * dt;
    if (std::abs(dy) > MOVE_EPSILON) {
        glm::vec3 testPos = pos;
        testPos.y += dy;

        if (!overlaps(testPos)) {
            pos.y = testPos.y;
        } else {
            const float safe = BinarySearchCollision(
                pos, glm::vec3(0.0f, dy > 0.0f ? 1.0f : -1.0f, 0.0f), std::abs(dy), overlaps
            );
            pos.y += (dy > 0.0f ? 1.0f : -1.0f) * safe;
            vel.linear.y = 0.0f;
        }
    }

    // ===== HORIZONTAL =====
    const float horizSpeed = glm::length(glm::vec2(vel.linear.x, vel.linear.z));
    if (horizSpeed > MOVE_EPSILON) {
        glm::vec3 targetPos = pos;
        targetPos.x += vel.linear.x * dt;
        targetPos.z += vel.linear.z * dt;

        if (!overlaps(targetPos)) {
            pos.x = targetPos.x;
            pos.z = targetPos.z;
        } else {
            const bool supported = overlaps(glm::vec3(pos.x, pos.y - GROUND_PROBE_DISTANCE, pos.z));
            const bool canStepUp =
                stepPhysics && stepPhysics->stepHeight > MOVE_EPSILON && groundPhysics &&
                (groundPhysics->onGround || (vel.linear.y <= 0.0f && supported));

            if (!canStepUp || !TryStepUp(vel, stepPhysics->stepHeight, dt, pos, overlaps)) {
                SlideHorizontal(pos, vel, dt, overlaps);
            }
        }
    }

    t.setPosition(pos - col.localOffset);

    if (groundPhysics) {
        UpdateGroundState(pos, half, vel, overlaps, groundPhysics);
    }
}

void PhysicsSystem::UpdateGroundState(
    const glm::vec3 &pos,
    const glm::vec3 &halfExtents,
    components::Velocity &vel,
    const VoxelOverlapProbe &overlaps,
    components::GroundPhysics *groundPhysics
) {
    const float feetY = pos.y - halfExtents.y;
    const glm::vec3 groundCheckPos(pos.x, feetY + halfExtents.y - GROUND_PROBE_DISTANCE, pos.z);

    groundPhysics->onGround = overlaps(groundCheckPos);
    if (vel.linear.y > 0.0f) {
        groundPhysics->onGround = false;
    }

    if (groundPhysics->onGround) {
        constexpr float friction = 0.8f;
        vel.linear.x *= friction;
        vel.linear.z *= friction;
    } else {
        vel.linear.x *= AIR_FRICTION;
        vel.linear.z *= AIR_FRICTION;
    }
}

bool PhysicsSystem::TryStepUp(
    const components::Velocity &vel,
    float stepHeight,
    float dt,
    glm::vec3 &currentPos,
    const VoxelOverlapProbe &overlaps
) {
    const glm::vec3 horizontalDisplacement(vel.linear.x * dt, 0.0f, vel.linear.z * dt);

    constexpr int steps = 4;
    for (int i = 1; i <= steps; ++i) {
        const float h = (stepHeight * i) / static_cast<float>(steps);
        glm::vec3 testPos = currentPos;
        testPos.y += h;

        if (overlaps(testPos)) {
            continue;
        }

        const glm::vec3 finalPos = testPos + horizontalDisplacement;
        if (!overlaps(finalPos)) {
            currentPos = finalPos;
            return true;
        }
    }

    return false;
}

void PhysicsSystem::SlideHorizontal(
    glm::vec3 &pos,
    components::Velocity &vel,
    float dt,
    const VoxelOverlapProbe &overlaps
) {
    glm::vec3 testPosX = pos;
    testPosX.x += vel.linear.x * dt;

    if (!overlaps(testPosX)) {
        pos.x = testPosX.x;
    } else {
        const float signX = (vel.linear.x > 0.0f) ? 1.0f : -1.0f;
        pos.x += signX * BinarySearchCollision(pos, glm::vec3(signX, 0.0f, 0.0f), std::abs(vel.linear.x * dt), overlaps);
        vel.linear.x = 0.0f;
    }

    glm::vec3 testPosZ = pos;
    testPosZ.z += vel.linear.z * dt;

    if (!overlaps(testPosZ)) {
        pos.z = testPosZ.z;
    } else {
        const float signZ = (vel.linear.z > 0.0f) ? 1.0f : -1.0f;
        pos.z += signZ * BinarySearchCollision(pos, glm::vec3(0.0f, 0.0f, signZ), std::abs(vel.linear.z * dt), overlaps);
        vel.linear.z = 0.0f;
    }
}

float PhysicsSystem::BinarySearchCollision(
    const glm::vec3 &startPos,
    const glm::vec3 &direction,
    float maxDistance,
    const VoxelOverlapProbe &overlaps
) const {
    float low = 0.0f;
    float high = maxDistance;

    constexpr int iterations = 8;
    for (int i = 0; i < iterations; ++i) {
        const float mid = (low + high) * 0.5f;
        if (!overlaps(startPos + direction * mid)) {
            low = mid;
        } else {
            high = mid;
        }
    }

    return low;
}

} // namespace ecs::systems
