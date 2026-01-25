// ecs/systems/PhysicsSystem.cpp
#include <ASCIICraft/ecs/systems/PhysicsSystem.hpp>

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/managers/PlayerManager.hpp>

namespace ecs::systems {

PhysicsSystem::PhysicsSystem(entt::registry &registry) noexcept
  : m_registry(registry)
  , m_accumulator(0.0f)
{}

void PhysicsSystem::Update() {
    float frameDt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

    m_accumulator += frameDt;
    
    // Clamp accumulator to prevent spiral of death
    constexpr float maxAccumulator = FixedDt * 5.0f;
    m_accumulator = std::min(m_accumulator, maxAccumulator);
    
    while (m_accumulator >= FixedDt) {
        Step(FixedDt);
        m_accumulator -= FixedDt;
    }
}

void PhysicsSystem::Step(float fixedDt) {
    const World* world = GetWorldPtr(m_registry);
    if (!world) {
        return;
    }

    // Integrate all entities with Transform + Velocity + Collider
    IntegrateEntities(fixedDt);

    // Readjust camera position based on new player position
    auto view = m_registry.view<managers::PlayerManager, components::PlayerCamera, components::Transform>();
    for (auto [ent, man, cam, t] : view.each()) {
        cam.camera->setCamPos(t.position + cam.PLAYER_EYE_HEIGHT);
    }
}

void PhysicsSystem::IntegrateEntities(float dt) {
    const World* world = GetWorldPtr(m_registry);
    if (!world) return;

    // Process all entities with Transform, Velocity, and Collider
    auto view = m_registry.view<components::Transform, components::Velocity, components::Collider>();
    
    for (auto [ent, t, v, col] : view.each()) {
        const auto *gravityComp = m_registry.try_get<components::Gravity>(ent);
        const auto *stepComp    = m_registry.try_get<components::StepPhysics>(ent);
        auto *groundComp        = m_registry.try_get<components::GroundPhysics>(ent);
        const auto *flyingComp  = m_registry.try_get<components::FlyingPhysics>(ent);
        const auto *playerMode  = m_registry.try_get<components::PlayerMode>(ent);

        // Apply gravity if applicable
        if (gravityComp) {
            const bool canFly = flyingComp && flyingComp->enabled;
            if (!canFly) {
                v.linear += gravityComp->acceleration * dt;
            }
        }

        v.ApplyDamping(dt);
        v.ClampSpeed();

        // Collision resolution (skip for spectator mode)
        const bool isSpectator = playerMode && playerMode->gamemode == GameMode::Spectator;
        if (!isSpectator && stepComp && groundComp) { 
            ResolveAABBAgainstWorld(ent, t, col, v, dt, stepComp, groundComp);
        }
    }
}

void PhysicsSystem::ResolveAABBAgainstWorld(
    entt::entity ent,
    components::Transform &t,
    components::Collider &col,
    components::Velocity &vel,
    float dt,
    const components::StepPhysics *stepPhysics,
    components::GroundPhysics *groundPhysics) {
    
    const World* world = GetWorldPtr(m_registry);
    if (!world) return;

    // Compute AABB center in world space
    glm::vec3 pos = t.position + col.localOffset;
    const glm::vec3 half = col.halfExtents;

    // Lambda to check if AABB overlaps any solid voxel
    auto overlapsVoxel = [&](const glm::vec3 &center) -> bool {
        const glm::vec3 min = center - half;
        const glm::vec3 max = center + half;

        const glm::ivec3 imin = glm::floor(min);
        const glm::ivec3 imax = glm::floor(max);

        for (int x = imin.x; x <= imax.x; ++x) {
            for (int y = imin.y; y <= imax.y; ++y) {
                for (int z = imin.z; z <= imax.z; ++z) {
                    if (world->GetChunkManager()->GetBlock({x, y, z}).type != BlockType::Air) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    // ===== VERTICAL MOVEMENT (Y-axis) =====
    constexpr float epsilon = 0.0001f;
    const float dy = vel.linear.y * dt;
    
    if (std::abs(dy) > epsilon) {
        glm::vec3 testPos = pos;
        testPos.y += dy;
        
        if (!overlapsVoxel(testPos)) {
            pos.y = testPos.y;
        } else {
            // Collision detected - slide to contact
            const float sign = (dy > 0.0f) ? 1.0f : -1.0f;
            const float safeDistance = BinarySearchCollision(pos, glm::vec3(0, sign, 0), std::abs(dy), overlapsVoxel);
            pos.y += sign * safeDistance;
            vel.linear.y = 0.0f;
        }
    }

    // ===== HORIZONTAL MOVEMENT (X and Z together) =====
    const glm::vec2 horizontalVel(vel.linear.x, vel.linear.z);
    const float horizontalSpeed = glm::length(horizontalVel);
    
    if (horizontalSpeed > epsilon) {
        glm::vec3 targetPos = pos;
        targetPos.x += vel.linear.x * dt;
        targetPos.z += vel.linear.z * dt;

        if (!overlapsVoxel(targetPos)) {
            // Free movement
            pos.x = targetPos.x;
            pos.z = targetPos.z;
        } else if (stepPhysics && stepPhysics->stepHeight > epsilon) {
            // Try step-up for entities with stepping ability
            if (!TryStepUp(t, col, vel, stepPhysics->stepHeight, dt, pos)) {
                // Step-up failed, slide along collision
                SlideHorizontal(pos, vel, dt, overlapsVoxel);
            }
        } else {
            // No stepping ability - slide along collision
            SlideHorizontal(pos, vel, dt, overlapsVoxel);
        }
    }

    // Commit final position back to transform
    t.setPosition(pos - col.localOffset);

    // ===== UPDATE GROUND STATE =====
    if (groundPhysics) {
        UpdateGroundState(pos, half, vel, overlapsVoxel, groundPhysics);
    }
}

void PhysicsSystem::UpdateGroundState(
    const glm::vec3 &pos,
    const glm::vec3 &halfExtents,
    components::Velocity &vel,
    const std::function<bool(const glm::vec3&)> &overlapsVoxel,
    components::GroundPhysics *groundPhysics) {
    
    // Check if on ground with slightly expanded downward check
    constexpr float groundCheckDistance = 0.05f;
    glm::vec3 groundCheckPos = pos;
    groundCheckPos.y -= groundCheckDistance;
    
    groundPhysics->onGround = overlapsVoxel(groundCheckPos);

    if (groundPhysics->onGround) {
        // Apply ground friction/slipperiness
        // TODO: Get actual friction value from block below
        constexpr float friction = 0.8f;
        
        vel.linear.x *= friction;
        vel.linear.z *= friction;
    }
}

bool PhysicsSystem::TryStepUp(
    components::Transform &t,
    const components::Collider &col,
    const components::Velocity &vel,
    float stepHeight,
    float dt,
    glm::vec3 &currentPos) {
    
    const World* world = GetWorldPtr(m_registry);
    if (!world) return false;
    
    const glm::vec3 half = col.halfExtents;

    // Compute horizontal displacement
    const glm::vec3 horizontalDisplacement(vel.linear.x * dt, 0.0f, vel.linear.z * dt);
    
    auto overlaps = [&](const glm::vec3 &center) -> bool {
        const glm::vec3 min = center - half;
        const glm::vec3 max = center + half;
        const glm::ivec3 imin = glm::floor(min);
        const glm::ivec3 imax = glm::floor(max);
        
        for (int x = imin.x; x <= imax.x; ++x) {
            for (int y = imin.y; y <= imax.y; ++y) {
                for (int z = imin.z; z <= imax.z; ++z) {
                    if (world->GetChunkManager()->GetBlock({x, y, z}).type != BlockType::Air) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    // Try stepping up at different heights
    constexpr int steps = 4;
    for (int i = 1; i <= steps; ++i) {
        const float h = (stepHeight * i) / static_cast<float>(steps);
        glm::vec3 testPos = currentPos;
        testPos.y += h;

        // Check if we can fit at this height
        if (overlaps(testPos)) continue;

        // Check if we can move horizontally at this height
        const glm::vec3 finalPos = testPos + horizontalDisplacement;
        if (!overlaps(finalPos)) {
            // Success! Apply the step-up and horizontal movement
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
    const std::function<bool(const glm::vec3&)> &overlapsVoxel) {
    
    // Try X-axis movement only
    glm::vec3 testPosX = pos;
    testPosX.x += vel.linear.x * dt;
    
    if (!overlapsVoxel(testPosX)) {
        pos.x = testPosX.x;
    } else {
        // Slide to contact on X
        const float signX = (vel.linear.x > 0.0f) ? 1.0f : -1.0f;
        const float safeX = BinarySearchCollision(pos, glm::vec3(signX, 0, 0), std::abs(vel.linear.x * dt), overlapsVoxel);
        pos.x += signX * safeX;
        vel.linear.x = 0.0f;
    }

    // Try Z-axis movement only
    glm::vec3 testPosZ = pos;
    testPosZ.z += vel.linear.z * dt;
    
    if (!overlapsVoxel(testPosZ)) {
        pos.z = testPosZ.z;
    } else {
        // Slide to contact on Z
        const float signZ = (vel.linear.z > 0.0f) ? 1.0f : -1.0f;
        const float safeZ = BinarySearchCollision(pos, glm::vec3(0, 0, signZ), std::abs(vel.linear.z * dt), overlapsVoxel);
        pos.z += signZ * safeZ;
        vel.linear.z = 0.0f;
    }
}

float PhysicsSystem::BinarySearchCollision(
    const glm::vec3 &startPos,
    const glm::vec3 &direction,
    float maxDistance,
    const std::function<bool(const glm::vec3&)> &overlapsVoxel) const {
    
    float low = 0.0f;
    float high = maxDistance;
    
    constexpr int iterations = 5;
    for (int i = 0; i < iterations; ++i) {
        const float mid = (low + high) * 0.5f;
        const glm::vec3 testPos = startPos + direction * mid;
        
        if (!overlapsVoxel(testPos)) {
            low = mid;
        } else {
            high = mid;
        }
    }
    
    return low;
}

} // namespace ecs::systems