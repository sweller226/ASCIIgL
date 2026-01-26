#include <ASCIICraft/ecs/managers/PlayerManager.hpp>

#include <ASCIIgL/util/Logger.hpp>

namespace ecs::managers {

PlayerManager::PlayerManager(entt::registry& registry) 
    : registry(registry) {

}

PlayerManager::~PlayerManager() {
    destroyPlayerEnt();
}

void PlayerManager::createPlayerEnt(const glm::vec3& startPosition, GameMode mode) {
    // Create entity
    p_ent = registry.create();

    // --- Core components ---
    auto& t       = registry.emplace<components::Transform>(p_ent);
    auto& vel     = registry.emplace<components::Velocity>(p_ent);
    auto& body    = registry.emplace<components::PhysicsBody>(p_ent);
    auto& step    = registry.emplace<components::StepPhysics>(p_ent);
    auto& grav    = registry.emplace<components::Gravity>(p_ent);
    auto& ground  = registry.emplace<components::GroundPhysics>(p_ent);
    auto& flying  = registry.emplace<components::FlyingPhysics>(p_ent);
    auto& ctrl    = registry.emplace<components::PlayerController>(p_ent);
    auto& jump    = registry.emplace<components::Jump>(p_ent);
    auto& cam     = registry.emplace<components::PlayerCamera>(p_ent);
    auto& pmode   = registry.emplace<components::PlayerMode>(p_ent);
    auto& col     = registry.emplace<components::Collider>(p_ent);

    // --- Transform ---
    t.setPosition(startPosition);

    // --- Velocity ---
    vel.maxSpeed = 100.0f;

    // --- Camera ---
    glm::vec3 eyePos = startPosition + glm::vec3(0, cam.PLAYER_EYE_HEIGHT, 0);
    cam.camera->setCamPos(eyePos);

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


void PlayerManager::destroyPlayerEnt() {
    if (p_ent != entt::null) registry.destroy(p_ent);
    p_ent = entt::null;
}

glm::vec3 PlayerManager::GetPosition() const {
    // Validate entity
    if (p_ent == entt::null || !registry.valid(p_ent)) {
        ASCIIgL::Logger::Error("PlayerManager::GetPosition: Player entity is invalid or null.");
        return glm::vec3(0.0f);
    }

    // Check component existence
    if (!registry.all_of<components::Transform>(p_ent)) {
        ASCIIgL::Logger::Error("PlayerManager::GetPosition: Transform component missing on player entity.");
        return glm::vec3(0.0f);
    }

    // Safe retrieval
    auto* t = registry.try_get<components::Transform>(p_ent);
    if (!t) {
        ASCIIgL::Logger::Error("PlayerManager::GetPosition: try_get returned NULL for Transform.");
        return glm::vec3(0.0f);
    }

    return t->position;
}

ASCIIgL::Camera3D* PlayerManager::GetCamera() {
    // Validate player entity
    if (p_ent == entt::null || !registry.valid(p_ent)) {
        ASCIIgL::Logger::Error("PlayerManager::GetCamera: Player entity is invalid or null.");
        return nullptr;
    }

    // Check if the component exists
    if (!registry.all_of<components::PlayerCamera>(p_ent)) {
        ASCIIgL::Logger::Error("PlayerManager::GetCamera: PlayerCamera component missing on player entity.");
        return nullptr;
    }

    // Retrieve the component safely
    auto* camComp = registry.try_get<components::PlayerCamera>(p_ent);
    if (!camComp) {
        ASCIIgL::Logger::Error("PlayerManager::GetCamera: try_get returned NULL for PlayerCamera.");
        return nullptr;
    }

    // Check the internal camera pointer
    if (!camComp->camera) {
        ASCIIgL::Logger::Error("PlayerManager::GetCamera: PlayerCamera component exists but camera pointer is NULL.");
        return nullptr;
    }

    return camComp->camera.get();
}


PlayerManager* GetPlayerPtr(entt::registry& registry) {
    // Try to find the PlayerManager in the context
    auto* pm = registry.ctx().find<PlayerManager>();

    if (!pm) {
        ASCIIgL::Logger::Error("GetPlayerPtr: PlayerManager not found in registry context.");
        return nullptr;
    }

    ASCIIgL::Logger::Debug("GetPlayerPtr: PlayerManager retrieved successfully");
    return pm; // already a pointer
}


}


