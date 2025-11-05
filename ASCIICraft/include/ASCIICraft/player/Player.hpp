#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/renderer/Screen.hpp>

#include <ASCIICraft/input_manager/InputManager.hpp>

// Forward declaration
class World;

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

class Player {
public:
    // Constructor/Destructor
    Player() = default;
    Player(const glm::vec3& startPosition, GameMode mode = GameMode::Spectator);
    ~Player() = default;

    // Core update functions
    void Update(World* world);
    void HandleInput(const InputManager& input);
    void UpdatePhysics(World* world);
    void UpdateCamera();

    // Movement
    void Move(const glm::vec3& direction);
    void Jump();
    // void StartSprinting();
    // void StopSprinting();
    // void StartSneaking();
    // void StopSneaking();
    // void ToggleFlight();
    void RecalculateCameraPosition();

    // Block interaction
    // void BreakBlock(World& world);
    // void PlaceBlock(World& world);
    // void SelectNextBlock();
    // void SelectPreviousBlock();
    // void SelectHotbarSlot(int slot);

    // Getters
    const glm::vec3& GetPosition() const { return position; }
    const glm::vec3& GetVelocity() const { return velocity; }
    const Camera3D& GetCamera() const { return camera; }
    
    GameMode GetGameMode() const { return gameMode; }
    MovementState GetMovementState() const { return movementState; }
    
    bool IsOnGround() const { return onGround; }
    bool IsFlying() const { return flying; }
    bool IsSprinting() const { return sprinting; }
    bool IsSneaking() const { return sneaking; }

    // Setters
    void SetPosition(const glm::vec3& pos);
    void SetGameMode(GameMode mode);

    // Inventory access
    // Inventory& GetInventory() { return *inventory; }
    // const Inventory& GetInventory() const { return *inventory; }

    // Respawn
    void Respawn(const glm::vec3& spawnPosition);

private:
    glm::vec3 position;
    glm::vec3 velocity;

    // Camera
    Camera3D camera;

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
    bool onGround;
    bool flying;
    bool sprinting;
    bool sneaking;
    bool jumpPressed;

    // Bounding box for collision
    glm::vec3 boundingBoxSize;
    glm::vec3 eyeOffset; // Offset from feet to eyes

    // Block interaction
    int selectedHotbarSlot;
    float blockBreakProgress;
    glm::ivec3 targetBlock;
    bool isBreakingBlock;

    // Timers
    float jumpCooldown;
    float lastOnGround;

    // Private helper functions
    void UpdateMovementState();
    void ApplyGravity();
    void HandleCollisions(World* world);
    float SweepAxis(const glm::vec3& testPosition, World* world, int axis) const;
    glm::vec3 GetBoundingBoxMin() const { return position; }
    glm::vec3 GetBoundingBoxMax() const { return position + boundingBoxSize; }
    
    // Input helpers
    void ProcessMovementInput(const InputManager& input);
    void ProcessCameraInput(const InputManager& input);
    // void ProcessActionInput(const InputManager& input, World* world);

    // Block interaction helpers
    // glm::ivec3 GetTargetBlock(const World& world) const;
    // bool CanReachBlock(const glm::ivec3& blockPos) const;
    // float GetBlockBreakTime() const;

    // Movement constants
    static constexpr float DEFAULT_WALK_SPEED = 4.317f;      // Blocks per second
    static constexpr float DEFAULT_RUN_SPEED = 5.612f;       // Blocks per second
    static constexpr float DEFAULT_SNEAK_SPEED = 1.295f;     // Blocks per second
    static constexpr float DEFAULT_FLY_SPEED = 10.89f;       // Blocks per second
    static constexpr float DEFAULT_JUMP_HEIGHT = 1.35f;      // Blocks
    static constexpr float DEFAULT_GRAVITY = -32.0f;         // Blocks per second squared
    static constexpr float DEFAULT_REACH = 5.0f;             // Blocks
    
    static constexpr float PLAYER_WIDTH = 0.6f;              // Blocks
    static constexpr float PLAYER_HEIGHT = 1.8f;             // Blocks
    static constexpr float PLAYER_EYE_HEIGHT = 1.62f;        // Blocks from feet to eyes

    static constexpr float JUMP_COOLDOWN_MAX = 0.2f;         // Seconds

    static constexpr float CAMERA_NEAR_PLANE = 0.1f;
    static constexpr float CAMERA_FAR_PLANE = 300.0f;
    static constexpr float FOV = 70.0f;                      // Field of view in degrees
};
