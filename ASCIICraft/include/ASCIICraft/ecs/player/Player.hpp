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

enum class GameMode {
    Survival,
    Spectator  // Spectator mode with noclip
};
        
enum class MovementState {
    Walking,
    Running,
    Sneaking,
    Flying,
    Falling,
};

namespace ecs::components {

struct PlayerManager {
  entt::entity player{entt::null};
  void create(entt::registry &r) {
    player = r.create();
    r.emplace<components::Transform>(player);
    r.emplace<components::Velocity>(player);
    r.emplace<components::PhysicsBody>(player);
    r.emplace<components::StepPhysics>(player);
    r.emplace<components::Gravity>(player, DEFAULT_GRAVITY);
    r.emplace<components::GroundPhysics>(player);
    r.emplace<components::FlyingPhysics>(player);
    r.emplace<components::PlayerController>(player);
  }
  
  void destroy(entt::registry &r) {
    if (player != entt::null) r.destroy(player);
    player = entt::null;
  }

  entt::entity get() const { return player; }

  
  static constexpr glm::vec3 DEFAULT_GRAVITY = glm::vec3(0.0f, -32.0f, 0.0f); // Blocks per second squared
};

}


class Player {
public:
    // Constructor/Destructor
    Player() = default;
    Player(const glm::vec3& startPosition, GameMode mode = GameMode::Spectator);
    ~Player() = default;

    // Core update functions
    void Update(World* world);
    void HandleInput(const ASCIIgL::InputManager& input);
    void UpdatePhysics(World* world);
    void UpdateCamera();

    // Movement
    void Move(const glm::vec3& direction);
    void Jump();

    // Getters
    const glm::vec3& GetPosition() const { return position; }
    const glm::vec3& GetVelocity() const { return velocity; }
    const ASCIIgL::Camera3D& GetCamera() const { return camera; }
    
    GameMode GetGameMode() const { return gameMode; }
    MovementState GetMovementState() const { return movementState; }
    
    bool IsOnGround() const { return onGround; }
    bool IsFlying() const { return flying; }
    bool IsSprinting() const { return sprinting; }
    bool IsSneaking() const { return sneaking; }

    // Setters
    void SetPosition(const glm::vec3& pos);
    void SetGameMode(GameMode mode);

    // Respawn
    void Respawn(const glm::vec3& spawnPosition);

private:
    glm::vec3 position;
    glm::vec3 velocity;

    // Camera
    ASCIIgL::Camera3D camera;

    // Physical properties
    float walkSpeed;
    float runSpeed;
    float sneakSpeed;
    float flySpeed;
    float jumpHeight;
    float gravity;
    float reach;        // Block interaction reach distance

    // State flags
    GameMode gameMode;
    MovementState movementState;
    
    // Private helper functions
    void UpdateMovementState();
    void ApplyGravity();
    void HandleCollisions(World* world);
    float SweepAxis(const glm::vec3& testPosition, World* world, int axis) const;
    glm::vec3 GetBoundingBoxMin() const { return position; }
    glm::vec3 GetBoundingBoxMax() const { return position + boundingBoxSize; }
    
    // Input helpers
    void ProcessMovementInput();
    void ProcessCameraInput();

    // Movement constants
    static constexpr float DEFAULT_WALK_SPEED = 4.317f;      // Blocks per second
    static constexpr float DEFAULT_RUN_SPEED = 5.612f;       // Blocks per second
    static constexpr float DEFAULT_SNEAK_SPEED = 1.295f;     // Blocks per second
    static constexpr float DEFAULT_FLY_SPEED = 10.89f;       // Blocks per second
    static constexpr float DEFAULT_JUMP_HEIGHT = 1.35f;      // Blocks
    static constexpr float DEFAULT_REACH = 5.0f;             // Blocks
    
    static constexpr float PLAYER_WIDTH = 0.6f;              // Blocks
    static constexpr float PLAYER_HEIGHT = 1.8f;             // Blocks

    static constexpr float JUMP_COOLDOWN_MAX = 0.2f;         // Seconds
};
