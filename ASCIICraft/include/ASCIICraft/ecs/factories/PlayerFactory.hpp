#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>

#include <entt/entt.hpp>

// components
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/Jump.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/Reach.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/PlayerInput.hpp>
#include <ASCIICraft/ecs/components/StepSoundState.hpp>

#include <ASCIICraft/ecs/factories/PlayerSpawnState.hpp>

namespace ecs::factories {

class PlayerFactory {
  public:
  PlayerFactory(entt::registry& registry);

  /// Spawns the player. \p spawn carries position, facing and game mode, so a
  /// restored save and a fresh spawn take the exact same path - in particular the
  /// game-mode switch below, which is the only place the mode's side effects live.
  void createPlayerEnt(const PlayerSpawnState& spawn);

  /// Convenience for callers that only have a position and a mode; facing defaults to
  /// the historical -Z.
  void createPlayerEnt(const glm::vec3& startPosition, GameMode mode);

  private:
  entt::registry& registry;

  static inline const glm::vec3 DEFAULT_GRAVITY               = glm::vec3(0.0f, -32.0f, 0.0f); // Blocks per second squared
  static constexpr GameMode     DEFAULT_GAMEMODE               = GameMode::Spectator;

  static inline const glm::vec3 DEFAULT_COLLIDER_HALFEXTENTS   = glm::vec3(0.3f, 0.9f, 0.3f);
  static inline const glm::vec3 DEFAULT_COLLIDER_OFFSET        = glm::vec3(0.0f, 0.9f, 0.0f);
  static constexpr uint32_t     DEFAULT_COLLIDER_LAYER         = 1;
  static constexpr uint32_t     DEFAULT_COLLIDER_MASK          = 0xFFFFFFFFu;
  static constexpr bool         DEFAULT_COLLIDER_DISABLED      = false;
};

}