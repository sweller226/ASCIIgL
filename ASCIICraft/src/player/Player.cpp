#include <ASCIICraft/player/Player.hpp>
#include <ASCIICraft/world/World.hpp>

#include <algorithm>
#include <cmath>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

Player::Player(const glm::vec3& startPosition, GameMode mode)
    : position(startPosition)
    , velocity(0.0f)
    , camera(startPosition + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f), FOV, 16.0f/9.0f, glm::vec2(0.0f, 0.0f), CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE)
    , walkSpeed(DEFAULT_WALK_SPEED)
    , runSpeed(DEFAULT_RUN_SPEED)
    , sneakSpeed(DEFAULT_SNEAK_SPEED)
    , flySpeed(DEFAULT_FLY_SPEED)
    , jumpHeight(DEFAULT_JUMP_HEIGHT)
    , gravity(DEFAULT_GRAVITY)
    , reach(DEFAULT_REACH)
    , gameMode(mode)
    , movementState(MovementState::Walking)
    , onGround(false)
    , flying(true) // Default flying state (overridden by game mode switch below)
    , sprinting(false)
    , sneaking(false)
    , jumpPressed(false)
    , boundingBoxSize(PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH)
    , eyeOffset(0.0f, PLAYER_EYE_HEIGHT, 0.0f)
    , selectedHotbarSlot(0)
    , blockBreakProgress(0.0f)
    , targetBlock(0)
    , isBreakingBlock(false)
    , jumpCooldown(0.0f)
    , lastOnGround(0.0f) {
    
    Logger::Info("Player created at position (" + 
                 std::to_string(position.x) + ", " + 
                 std::to_string(position.y) + ", " + 
                 std::to_string(position.z) + ")");

    switch (mode) {
        case GameMode::Survival:
            flying = false;
            movementState = MovementState::Walking;
            break;
        case GameMode::Spectator:
            flying = true;  // Enable noclip for spectator mode
            movementState = MovementState::Flying;
            break;
    }
    
    UpdateCamera();
}

void Player::Update(World* world) {
    // Update movement state
    UpdateMovementState();
    
    // Update camera
    UpdateCamera();

    UpdatePhysics(world);
    
    // Update timers
    jumpCooldown = std::max(0.0f, jumpCooldown - FPSClock::GetInst().GetDeltaTime());
    
    if (onGround) {
        lastOnGround = 0.0f;
    } else {
        lastOnGround += FPSClock::GetInst().GetDeltaTime();
    }

    // Reset jump pressed flag
    jumpPressed = false;
}

void Player::UpdatePhysics(World* world) {
    // Skip physics for spectator mode (noclip)
    if (gameMode == GameMode::Spectator || flying) {
        onGround = false;
        velocity.y = 0.0f;
        return;
    }
    
    // Apply gravity for survival mode
    ApplyGravity();
    
    // Apply velocity to position with collision detection
    HandleCollisions(world);
    
    // Note: onGround is set in HandleCollisions when hitting floor
}

void Player::HandleInput(const InputManager& input) {
    ProcessMovementInput(input);
    ProcessCameraInput(input);
}

void Player::UpdateCamera() {
    // Update camera position to player eye position
    glm::vec3 eyePosition = position + eyeOffset;
    camera.setCamPos(eyePosition);
    
    // Smoothly adjust FOV when sprinting (slight increase for speed effect)
    float targetFOV = FOV;
    if (sprinting) {
        targetFOV = FOV + 10.0f;  // +10 degrees when sprinting
    }
    
    // Smoothly interpolate FOV towards target (smooth transition)
    float currentFOV = camera.GetFov();
    float fovTransitionSpeed = 8.0f; // How fast FOV changes (higher = faster)
    float deltaTime = FPSClock::GetInst().GetDeltaTime();
    
    // Lerp FOV: currentFOV + (targetFOV - currentFOV) * speed * deltaTime
    float newFOV = currentFOV + (targetFOV - currentFOV) * fovTransitionSpeed * deltaTime;
    
    // Update camera FOV
    camera.SetFov(newFOV);
}

void Player::Move(const glm::vec3& direction) {
    // Get current movement speed based on state
    float currentSpeed = walkSpeed;

    Logger::Info("Player Move: direction=(" + 
                  std::to_string(direction.x) + ", " + 
                  std::to_string(direction.y) + ", " + 
                  std::to_string(direction.z) + "), state=" + 
                  std::to_string(static_cast<int>(movementState)) + 
                  ", flying=" + std::to_string(flying) + 
                  ", gameMode=" + std::to_string(static_cast<int>(gameMode)));
    
    switch (movementState) {
        case MovementState::Running:
            currentSpeed = runSpeed;
            break;
        case MovementState::Sneaking:
            currentSpeed = sneakSpeed;
            break;
        case MovementState::Flying:
            currentSpeed = flySpeed;
            break;
        default:
            currentSpeed = walkSpeed;
            break;
    }
    
    if (flying || gameMode == GameMode::Spectator) {
        // Free flight movement (spectator noclip) - direct position change
        glm::vec3 movement = direction * currentSpeed * FPSClock::GetInst().GetDeltaTime();
        position += movement;
        velocity = movement / FPSClock::GetInst().GetDeltaTime();
        Logger::Info("Spectator mode: moved by (" + std::to_string(movement.x) + ", " + std::to_string(movement.y) + ", " + std::to_string(movement.z) + ")");
    } else {
        // Ground-based movement for survival - set horizontal velocity
        // Vertical velocity is controlled by gravity and jumping
        // Note: direction should already be normalized by ProcessMovementInput
        velocity.x = direction.x * currentSpeed;
        velocity.z = direction.z * currentSpeed;
        Logger::Info("Survival mode: velocity set to (" + std::to_string(velocity.x) + ", " + std::to_string(velocity.y) + ", " + std::to_string(velocity.z) + ")");
    }

    RecalculateCameraPosition();
}

void Player::RecalculateCameraPosition() {
    camera.setCamPos(position + eyeOffset);
}

void Player::SetPosition(const glm::vec3& pos) {
    position = pos;
    UpdateCamera();
}

void Player::SetGameMode(GameMode mode) {
    gameMode = mode;
    
    // Update movement capabilities based on game mode
    switch (mode) {
        case GameMode::Survival:
            flying = false;
            movementState = MovementState::Walking;
            break;
        case GameMode::Spectator:
            flying = true;  // Enable noclip for spectator mode
            movementState = MovementState::Flying;
            break;
    }
    
    Logger::Info("Player game mode changed to " + std::to_string(static_cast<int>(mode)));
}

void Player::Respawn(const glm::vec3& spawnPosition) {
    SetPosition(spawnPosition);
    velocity = glm::vec3(0.0f);
    
    // Reset player state
    movementState = (gameMode == GameMode::Spectator) ? MovementState::Flying : MovementState::Walking;
    flying = (gameMode == GameMode::Spectator);
    onGround = false;
    sprinting = false;
    sneaking = false;
    
    // Reset timers
    jumpCooldown = 0.0f;
    lastOnGround = 0.0f;
    blockBreakProgress = 0.0f;
    isBreakingBlock = false;
    
    Logger::Info("Player respawned at (" + 
                 std::to_string(spawnPosition.x) + ", " + 
                 std::to_string(spawnPosition.y) + ", " + 
                 std::to_string(spawnPosition.z) + ")");
}

void Player::ApplyGravity() {
    // Don't apply gravity in spectator/flying mode
    if (flying || gameMode == GameMode::Spectator) {
        return;
    }
    
    // Apply gravity acceleration
    velocity.y += gravity * FPSClock::GetInst().GetDeltaTime();
    
    // Terminal velocity cap (prevents falling through blocks at high speeds)
    const float TERMINAL_VELOCITY = -78.4f; // Minecraft's terminal velocity
    velocity.y = std::max(velocity.y, TERMINAL_VELOCITY);
}

void Player::HandleCollisions(World* world) {
    if (!world) return;  // Null check
    
    float deltaTime = FPSClock::GetInst().GetDeltaTime();
    
    Logger::Info("HandleCollisions: velocity=(" + std::to_string(velocity.x) + ", " + std::to_string(velocity.y) + ", " + std::to_string(velocity.z) + "), deltaTime=" + std::to_string(deltaTime));
    
    // Apply velocity with collision detection on each axis separately
    // This prevents getting stuck in corners
    
    // X-axis collision
    glm::vec3 testPos = position;
    testPos.x += velocity.x * deltaTime;
    if (!CheckCollision(testPos, world)) {
        position.x = testPos.x;
        Logger::Info("X: Moved from " + std::to_string(position.x - velocity.x * deltaTime) + " to " + std::to_string(position.x));
    } else {
        Logger::Info("X: COLLISION DETECTED at " + std::to_string(testPos.x));
        velocity.x = 0.0f;
    }
    
    // Y-axis collision
    testPos = position;
    testPos.y += velocity.y * deltaTime;
    if (!CheckCollision(testPos, world)) {
        position.y = testPos.y;
        // If moving upward or no collision, we're not on ground
        if (velocity.y >= 0.0f) {
            onGround = false;
        }
    } else {
        Logger::Info("Y: COLLISION DETECTED at " + std::to_string(testPos.y));
        // Hit ceiling or floor - stop vertical movement
        if (velocity.y < 0.0f) {
            // Hitting ground while falling
            onGround = true;
            Logger::Info("Set onGround=true (hit floor)");
        } else {
            // Hit ceiling while jumping/rising
            onGround = false;
        }
        velocity.y = 0.0f;
    }
    
    // Z-axis collision
    testPos = position;
    testPos.z += velocity.z * deltaTime;
    if (!CheckCollision(testPos, world)) {
        position.z = testPos.z;
        Logger::Info("Z: Moved from " + std::to_string(position.z - velocity.z * deltaTime) + " to " + std::to_string(position.z));
    } else {
        Logger::Info("Z: COLLISION DETECTED at " + std::to_string(testPos.z));
        velocity.z = 0.0f;
    }
    
    Logger::Info("Final position: (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")");
}

bool Player::CheckCollision(const glm::vec3& testPosition, World* world) const {
    if (!world) return false;  // Null check
    
    // Get player bounding box at test position
    // Center the box horizontally around the player position
    float halfWidth = boundingBoxSize.x / 2.0f;
    float halfDepth = boundingBoxSize.z / 2.0f;
    
    glm::vec3 min = testPosition + glm::vec3(-halfWidth, 0.0f, -halfDepth);
    glm::vec3 max = testPosition + glm::vec3(halfWidth, boundingBoxSize.y, halfDepth);
    
    // Check all blocks that the bounding box overlaps
    int minX = static_cast<int>(std::floor(min.x));
    int minY = static_cast<int>(std::floor(min.y));
    int minZ = static_cast<int>(std::floor(min.z));
    int maxX = static_cast<int>(std::floor(max.x));
    int maxY = static_cast<int>(std::floor(max.y));
    int maxZ = static_cast<int>(std::floor(max.z));
    
    // Check each block in the bounding box range
    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                Block block = world->GetBlock(x, y, z);
                
                // Check if block is solid (not air)
                if (block.type != BlockType::Air) {
                    return true; // Collision detected
                }
            }
        }
    }
    
    return false; // No collision
}

void Player::Jump() {
    Logger::Info("Jump() called - onGround=" + std::to_string(onGround) + 
                 ", jumpCooldown=" + std::to_string(jumpCooldown) + 
                 ", flying=" + std::to_string(flying));
    
    // Can only jump if on ground (no cooldown needed for hold-to-jump)
    if (onGround && jumpCooldown <= 0.0f && !flying) {
        // Calculate jump velocity from jump height
        // v = sqrt(2 * g * h)
        float jumpVelocity = std::sqrt(2.0f * std::abs(gravity) * jumpHeight);
        velocity.y = jumpVelocity;
        
        onGround = false;
        jumpCooldown = 0.2f; 
        
        Logger::Info("Player jumped! velocity.y=" + std::to_string(jumpVelocity));
    } else {
        Logger::Info("Jump FAILED - conditions not met");
    }
}

void Player::UpdateMovementState() {
    if (sprinting) {
        movementState = MovementState::Running;
    } else if (sneaking) {
        movementState = MovementState::Sneaking;
    } else {
        movementState = MovementState::Walking;
    }

    if (flying) {
        movementState = MovementState::Flying;
    }
}

void Player::ProcessMovementInput(const InputManager& input) {
    glm::vec3 moveDirection(0.0f);

    // Get camera directions for movement
    glm::vec3 forward = camera.getCamFrontNoY();
    glm::vec3 right = camera.getCamRightNoY();
    
    // Normalize forward and right for horizontal movement
    if (!flying) {
        forward.y = 0.0f;
        right.y = 0.0f;
        forward = glm::normalize(forward);
        right = glm::normalize(right);
    }
    
    // Process WASD movement
    if (input.IsActionHeld("move_forward")) {
        moveDirection += forward;
    }
    if (input.IsActionHeld("move_backward")) {
        moveDirection -= forward;
    }
    if (input.IsActionHeld("move_left")) {
        moveDirection -= right;
    }
    if (input.IsActionHeld("move_right")) {
        moveDirection += right;
    }
    
    // Process vertical movement for spectator mode (noclip)
    if (flying || gameMode == GameMode::Spectator) {
        if (input.IsActionHeld("jump")) {
            moveDirection.y += 1.0f;
        }
        if (input.IsActionHeld("sneak")) {
            moveDirection.y -= 1.0f;
        }
    } else {
        // Jump for survival mode (can hold to keep jumping when landing)
        if (input.IsActionHeld("jump")) {
            Jump();
        }
        
        // Sprinting (only in survival, only when moving)
        // Can sprint while jumping/in air - maintains momentum
        if (input.IsActionHeld("sprint") && glm::length(moveDirection) > 0.0f) {
            sprinting = true;
        } else {
            sprinting = false;
        }
    }
    
    // Normalize movement direction if needed
    if (glm::length(moveDirection) > 1.0f) {
        moveDirection = glm::normalize(moveDirection);
    }
    
    // Apply movement (always call Move, even with zero direction for survival mode)
    Move(moveDirection);
}

void Player::ProcessCameraInput(const InputManager& input) {
    // Use keyboard controls for camera movement (arrow keys only)
    
    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;
    
    // Arrow keys for camera rotation
    if (input.IsActionHeld("camera_left")) {
        yawDelta -= input.GetMouseSensitivity() * FPSClock::GetInst().GetDeltaTime();
    }
    if (input.IsActionHeld("camera_right")) {
        yawDelta += input.GetMouseSensitivity() * FPSClock::GetInst().GetDeltaTime();
    }
    if (input.IsActionHeld("camera_up")) {
        pitchDelta += input.GetMouseSensitivity() * FPSClock::GetInst().GetDeltaTime();
    }
    if (input.IsActionHeld("camera_down")) {
        pitchDelta -= input.GetMouseSensitivity() * FPSClock::GetInst().GetDeltaTime();
    }
    
    // Update camera direction
    camera.setCamDir(camera.GetYaw() + yawDelta, camera.GetPitch() + pitchDelta);
}

// void Player::ProcessActionInput(const InputManager& input, World& world) {
//     // Process game mode toggle (for testing - remove later)
//     // if (input.IsActionJustPressed("toggle_gamemode")) {
//     //     GameMode newMode = (gameMode == GameMode::Creative) ? GameMode::Survival : GameMode::Creative;
//     //     SetGameMode(newMode);
//     // }
    
//     // Block interaction
//     if (input.IsActionPressed("break_block")) {
//         // Q key - Left click replacement for breaking blocks
//         // TODO: Implement block breaking when BreakBlock method is available
//         Logger::Debug("Break block action pressed");
//     }
    
//     if (input.IsActionPressed("place_block")) {
//         // R key - Right click replacement for placing blocks
//         // TODO: Implement block placing when PlaceBlock method is available
//         Logger::Debug("Place block action pressed");
//     }
    
//     // Inventory toggle
//     if (input.IsActionPressed("open_inventory")) {
//         Logger::Debug("Inventory toggle pressed");
//     }
// }