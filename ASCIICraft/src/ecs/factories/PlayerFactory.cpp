#include <ASCIICraft/ecs/factories/PlayerFactory.hpp>

#include <ASCIIgL/util/Logger.hpp>

namespace ecs::factories {

PlayerFactory::PlayerFactory(entt::registry& registry) 
    : registry(registry) {}

void PlayerFactory::createPlayerEnt(const glm::vec3& position, GameMode mode) {
    // Create entity
    entt::entity p_ent = registry.create();
    
    // --- Core components ---
    auto& t       = registry.emplace<components::Transform>(p_ent);
    registry.emplace<components::PlayerTag>(p_ent);  // Tag component (empty struct)
    auto& vel     = registry.emplace<components::Velocity>(p_ent);
    auto& body    = registry.emplace<components::PhysicsBody>(p_ent);
    auto& grav    = registry.emplace<components::Gravity>(p_ent);
    auto& step    = registry.emplace<components::StepPhysics>(p_ent);
    auto& ground  = registry.emplace<components::GroundPhysics>(p_ent);
    auto& flying  = registry.emplace<components::FlyingPhysics>(p_ent);
    auto& ctrl    = registry.emplace<components::PlayerController>(p_ent);
    auto& jump    = registry.emplace<components::Jump>(p_ent);
    auto& cam     = registry.emplace<components::PlayerCamera>(p_ent);
    auto& pmode   = registry.emplace<components::PlayerMode>(p_ent);
    auto& col     = registry.emplace<components::Collider>(p_ent);
    auto& head    = registry.emplace<components::Head>(p_ent);
    auto& reach   = registry.emplace<components::Reach>(p_ent);
    auto& input   = registry.emplace<components::PlayerInput>(p_ent);

    // --- Transform ---
    t.setPosition(position);

    // --- Velocity ---
    vel.maxSpeed = 100.0f;

    // --- Camera ---
    glm::vec3 eyePos = position + glm::vec3(0, cam.PLAYER_EYE_HEIGHT, 0);
    glm::vec3 lookDir = glm::vec3(0.0f, 0.0f, -1.0f);
    cam.camera.setCamPos(eyePos);
    cam.camera.setCamDir(lookDir);

    // --- Head ---
    head.lookDir = lookDir;
    head.relativePos = glm::vec3(0, cam.PLAYER_EYE_HEIGHT, 0);

    // --- Reach ---
    reach.reach = 5.0f;

    // --- Collider defaults ---
    col.halfExtents = DEFAULT_COLLIDER_HALFEXTENTS;
    col.localOffset = DEFAULT_COLLIDER_OFFSET;
    col.layer       = DEFAULT_COLLIDER_LAYER;
    col.mask        = DEFAULT_COLLIDER_MASK;
    col.disabled    = DEFAULT_COLLIDER_DISABLED;

    // --- Gravity ---
    grav.acceleration = DEFAULT_GRAVITY;

    // --- Player mode ---
    pmode.gamemode = mode;

    // --- Movement state based on mode ---
    switch (mode) {
        case GameMode::Survival:
            ctrl.movementState = MovementState::Walking;
            flying.enabled = false;
            col.disabled = false;
            grav.enabled = true;
            break;
        case GameMode::Spectator:
            ctrl.movementState = MovementState::Flying;
            flying.enabled = true;
            col.disabled = true;
            grav.enabled = false;
            break;
    }
}

}
