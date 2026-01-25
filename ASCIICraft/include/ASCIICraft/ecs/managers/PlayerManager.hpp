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
#include <ASCIICraft/ecs/components/PlayerMode.hpp>

namespace ecs::managers {

class PlayerManager {
  public:
  PlayerManager(entt::registry& registry);
  ~PlayerManager();

  void createPlayerEnt();
  void initializePlayerEnt(const glm::vec3& startPosition, GameMode mode);
  void destroyPlayerEnt();

  entt::entity getPlayerEnt() const { return p_ent; }

  glm::vec3 GetPosition() const;
  ASCIIgL::Camera3D* GetCamera();

  private:
  entt::entity p_ent{entt::null};
  entt::registry& registry;

  static constexpr glm::vec3 DEFAULT_GRAVITY = glm::vec3(0.0f, -32.0f, 0.0f); // Blocks per second squared
  static constexpr GameMode DEFAULT_GAMEMODE = GameMode::Spectator;
};

PlayerManager* GetPlayerPtr(entt::registry& registry);

}