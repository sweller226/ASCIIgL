// ecs/systems/PhysicsSystem.cpp
#include <ASCIICraft/ecs/systems/PhysicsSystem.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerManager.hpp>

namespace ecs::systems {

PhysicsSystem::PhysicsSystem(entt::registry &registry) noexcept
  : m_registry(registry)
{}

void PhysicsSystem::Update(float frameDt) {
    m_accumulator += frameDt;
    
    // Clamp accumulator to prevent spiral of death
    if (m_accumulator > FixedDt * 5.0f) {
        m_accumulator = FixedDt * 5.0f;
    }
    
    while (m_accumulator >= FixedDt) {
        Step(FixedDt);
        m_accumulator -= FixedDt;
    }
}

void PhysicsSystem::Step(float fixedDt) {
    if (!m_registry.ctx().contains<World>()) {
        return;
    }

    // Integrate all entities with Transform + Velocity + Collider
    IntegrateEntities(fixedDt);

    // readjusting camera position based on new player position
    auto view = m_registry.view<components::PlayerManager, components::PlayerCamera, components::Transform>();
    for (auto [ent, man, cam, t] : view.each()) {
        auto *cameraComp = m_registry.try_get<components::PlayerCamera>(ent);

        if (cameraComp) {
            cameraComp->camera->setCamPos(t.position + cam.PLAYER_EYE_HEIGHT);
        }
    }
}

void PhysicsSystem::IntegrateEntities(float dt) {
    const auto world = m_registry.ctx<std::shared_ptr<World>>();

    // Process all entities with Transform, Velocity, and Collider
    auto view = m_registry.view<components::Transform, components::Velocity, components::Collider>();
    
    for (auto [ent, t, v, col] : view.each()) {
        // ent is entt::entity, t/v/col are references to components
        auto *gravityComp = m_registry.try_get<components::Gravity>(ent);
        auto *stepComp    = m_registry.try_get<components::StepPhysics>(ent);
        auto *groundComp  = m_registry.try_get<components::GroundPhysics>(ent);
        auto *flyingComp  = m_registry.try_get<components::FlyingPhysics>(ent);
        auto *playerMode  = m_registry.try_get<components::PlayerMode>(ent);

        if (gravityComp) {
            bool canFly = flyingComp && flyingComp->enabled;
            if (!canFly) v.linear += gravityComp->acceleration * dt;
        }

        v.ApplyDamping(dt);
        v.ClampSpeed();

        if (!(playerMode->gamemode == GameMode::Spectator) && stepComp && groundComp) { 
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
    components::StepPhysics *stepPhysics,
    components::GroundPhysics *groundPhysics) {
    
    const auto world = m_registry.ctx<std::shared_ptr<World>>();

    // Compute AABB center in world space
    glm::vec3 pos = t.position + col.localOffset;
    const glm::vec3 half = col.halfExtents;

    // Lambda to check if AABB overlaps any solid voxel
    auto overlapsVoxel = [&](const glm::vec3 &center) -> bool {
        glm::vec3 min = center - half;
        glm::vec3 max = center + half;

        glm::ivec3 imin = glm::floor(min);
        glm::ivec3 imax = glm::floor(max);

        for (int x = imin.x; x <= imax.x; ++x) {
            for (int y = imin.y; y <= imax.y; ++y) {
                for (int z = imin.z; z <= imax.z; ++z) {
                    if (world.IsSolidBlockAt({x, y, z})) return true;
                }
            }
        }
        return false;
    };

    // Store initial vertical velocity for fall distance tracking
    float initialVerticalVel = vel.linear.y;

    // ===== VERTICAL MOVEMENT (Y-axis) =====
    float dy = vel.linear.y * dt;
    if (std::abs(dy) > 0.0001f) {
        glm::vec3 testPos = pos;
        testPos.y += dy;
        
        if (!overlapsVoxel(testPos)) {
            pos.y = testPos.y;
        } else {
            // Collision detected - slide to contact
            float sign = (dy > 0.0f) ? 1.0f : -1.0f;
            float safeDistance = BinarySearchCollision(pos, glm::vec3(0, sign, 0), std::abs(dy), overlapsVoxel);
            pos.y += sign * safeDistance;
            vel.linear.y = 0.0f;
        }
    }

    // ===== HORIZONTAL MOVEMENT (X and Z together) =====
    glm::vec2 horizontalVel(vel.linear.x, vel.linear.z);
    float horizontalSpeed = glm::length(horizontalVel);
    
    if (horizontalSpeed > 0.0001f) {
        glm::vec3 targetPos = pos;
        targetPos.x += vel.linear.x * dt;
        targetPos.z += vel.linear.z * dt;

        if (!overlapsVoxel(targetPos)) {
            // Free movement
            pos.x = targetPos.x;
            pos.z = targetPos.z;
        } else if (stepPhysics && stepPhysics->stepHeight > 0.0f) {
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

    // ===== UPDATE GROUND STATE (for entities that care about it) =====
    if (groundPhysics) {
        // Check if on ground with slightly expanded downward check
        const float groundCheckDistance = 0.05f;
        glm::vec3 groundCheckPos = pos;
        groundCheckPos.y -= groundCheckDistance;
        
        bool wasOnGround = groundPhysics->onGround;
        groundPhysics->onGround = overlapsVoxel(groundCheckPos);

        if (groundPhysics->onGround) {
            // Apply ground friction/slipperiness
            glm::ivec3 blockBelow = glm::floor(pos - glm::vec3(0, half.y + 0.1f, 0));
            float friction = world.GetBlockFriction(blockBelow);
            
            vel.linear.x *= friction;
            vel.linear.z *= friction;
        }
    }
}

bool PhysicsSystem::TryStepUp(
    components::Transform &t,
    components::Collider &col,
    components::Velocity &vel,
    float stepHeight,
    float dt,
    glm::vec3 &currentPos) {
    
    const auto world = m_registry.ctx<std::shared_ptr<World>>();
    const glm::vec3 half = col.halfExtents;

    // Compute horizontal displacement
    glm::vec3 horizontalDisplacement(vel.linear.x * dt, 0.0f, vel.linear.z * dt);
    
    auto overlaps = [&](const glm::vec3 &center) -> bool {
        glm::vec3 min = center - half;
        glm::vec3 max = center + half;
        glm::ivec3 imin = glm::floor(min);
        glm::ivec3 imax = glm::floor(max);
        
        for (int x = imin.x; x <= imax.x; ++x)
            for (int y = imin.y; y <= imax.y; ++y)
                for (int z = imin.z; z <= imax.z; ++z)
                    if (world.IsSolidBlockAt({x, y, z})) return true;
        return false;
    };

    // Try stepping up at different heights
    const int steps = 4;
    for (int i = 1; i <= steps; ++i) {
        float h = (stepHeight * i) / float(steps);
        glm::vec3 testPos = currentPos;
        testPos.y += h;

        // Check if we can fit at this height
        if (overlaps(testPos)) continue;

        // Check if we can move horizontally at this height
        glm::vec3 finalPos = testPos + horizontalDisplacement;
        if (!overlaps(finalPos)) {
            // Success! Apply the step-up and horizontal movement
            currentPos = finalPos;
            t.translate({0.0f, h, 0.0f});
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
        float signX = (vel.linear.x > 0.0f) ? 1.0f : -1.0f;
        float safeX = BinarySearchCollision(pos, glm::vec3(signX, 0, 0), std::abs(vel.linear.x * dt), overlapsVoxel);
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
        float signZ = (vel.linear.z > 0.0f) ? 1.0f : -1.0f;
        float safeZ = BinarySearchCollision(pos, glm::vec3(0, 0, signZ), std::abs(vel.linear.z * dt), overlapsVoxel);
        pos.z += signZ * safeZ;
        vel.linear.z = 0.0f;
    }
}

float PhysicsSystem::BinarySearchCollision(
    const glm::vec3 &startPos,
    const glm::vec3 &direction,
    float maxDistance,
    const std::function<bool(const glm::vec3&)> &overlapsVoxel) {
    
    float low = 0.0f;
    float high = maxDistance;
    
    for (int i = 0; i < 5; ++i) {
        float mid = (low + high) * 0.5f;
        glm::vec3 testPos = startPos + direction * mid;
        
        if (!overlapsVoxel(testPos)) {
            low = mid;
        } else {
            high = mid;
        }
    }
    
    return low;
}

} // namespace ecs::systems