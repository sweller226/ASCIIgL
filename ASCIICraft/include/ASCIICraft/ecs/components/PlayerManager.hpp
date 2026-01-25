#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/Jump.hpp>

enum class GameMode {
    Survival,
    Spectator  // Spectator mode with noclip
};
        
namespace ecs::components {

struct PlayerMode {
  GameMode gamemode;
};

struct PlayerManager {
  entt::entity p_ent{entt::null};
  entt::registry *registry;

  void create(entt::registry &r) {
    registry = &r;
    p_ent = r.create();
    r.emplace<components::Transform>(p_ent);
    r.emplace<components::Velocity>(p_ent);
    r.emplace<components::PhysicsBody>(p_ent);
    r.emplace<components::StepPhysics>(p_ent);
    r.emplace<components::Gravity>(p_ent, DEFAULT_GRAVITY);
    r.emplace<components::GroundPhysics>(p_ent);
    r.emplace<components::FlyingPhysics>(p_ent);
    r.emplace<components::PlayerController>(p_ent);
    r.emplace<components::Jump>(p_ent);
    r.emplace<components::PlayerMode>(p_ent);
  }

  void initialize(const glm::vec3& startPosition, GameMode mode) {
    // required components
    if (!registry->all_of<
        components::PlayerController
      >(p_ent)) return;

    auto &ctrl = registry->get<components::PlayerController>(p_ent);

    switch (mode) {
        case GameMode::Survival:
            ctrl.movementState = MovementState::Walking;
            break;
        case GameMode::Spectator:
            ctrl.movementState = MovementState::Flying;
            break;
    }
  }
  
  void destroy(entt::registry &r) {
    if (p_ent != entt::null) r.destroy(p_ent);
    p_ent = entt::null;
  }

  entt::entity get() const { return p_ent; }

  static constexpr glm::vec3 DEFAULT_GRAVITY = glm::vec3(0.0f, -32.0f, 0.0f); // Blocks per second squared
  static constexpr GameMode DEFAULT_GAMEMODE = GameMode::Spectator;
};

}