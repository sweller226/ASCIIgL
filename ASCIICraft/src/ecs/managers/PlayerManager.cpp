#include <ASCIICraft/ecs/managers/PlayerManager.hpp>

namespace ecs::managers {

PlayerManager::PlayerManager(entt::registry& registry) 
    : registry(registry) {

}

PlayerManager::~PlayerManager() {
    destroyPlayerEnt();
}

void PlayerManager::createPlayerEnt() {
    p_ent = registry.create();
    registry.emplace<components::Transform>(p_ent);
    registry.emplace<components::Velocity>(p_ent);
    registry.emplace<components::PhysicsBody>(p_ent);
    registry.emplace<components::StepPhysics>(p_ent);
    registry.emplace<components::Gravity>(p_ent, DEFAULT_GRAVITY);
    registry.emplace<components::GroundPhysics>(p_ent);
    registry.emplace<components::FlyingPhysics>(p_ent);
    registry.emplace<components::PlayerController>(p_ent);
    registry.emplace<components::Jump>(p_ent);
    registry.emplace<components::PlayerMode>(p_ent);
}

void PlayerManager::initializePlayerEnt(const glm::vec3& startPosition, GameMode mode) {
    // required components
    if (!registry.all_of<
        components::PlayerController
      >(p_ent)) return;

    auto &ctrl = registry.get<components::PlayerController>(p_ent);

    switch (mode) {
        case GameMode::Survival:
            ctrl.movementState = MovementState::Walking;
            break;
        case GameMode::Spectator:
            ctrl.movementState = MovementState::Flying;
            break;
    }
}

void PlayerManager::destroyPlayerEnt() {
    if (p_ent != entt::null) registry.destroy(p_ent);
    p_ent = entt::null;
}

glm::vec3 PlayerManager::GetPosition() const {
    auto &t = registry.get<components::Transform>(p_ent);
    return t.position;
}

ASCIIgL::Camera3D* PlayerManager::GetCamera() {
    auto &cam = registry.get<components::PlayerCamera>(p_ent);
    return cam.camera.get();
}

PlayerManager* GetPlayerPtr(entt::registry& registry) {
    if (!registry.ctx().contains<std::unique_ptr<PlayerManager>>()) return nullptr;
    return registry.ctx().get<std::unique_ptr<PlayerManager>>().get();
}

}


