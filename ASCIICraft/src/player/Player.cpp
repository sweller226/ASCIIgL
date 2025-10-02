#include <ASCIICraft/player/Player.hpp>

#include <algorithm>
#include <cmath>

#include <ASCIIgL/engine/Logger.hpp>

Player::Player(const glm::vec3& startPosition, GameMode mode)
    : position(startPosition)
    , velocity(0.0f)
    , camera(startPosition + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f), 70.0f, 16.0f/9.0f, glm::vec2(0.0f, 0.0f), 0.1f, 1000.0f)
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
    , flying(true) // Start on ground in survival mode
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
    
    UpdateCamera();
}

void Player::Update() {
    // Update movement state
    UpdateMovementState();
    
    // Update camera
    UpdateCamera();
    
    // Update timers
    jumpCooldown = std::max(0.0f, jumpCooldown - Screen::GetInst().GetDeltaTime());
    lastOnGround += Screen::GetInst().GetDeltaTime();

    // Reset jump pressed flag
    jumpPressed = false;
}

void Player::HandleInput(const InputManager& input) {
    ProcessMovementInput(input);
    ProcessCameraInput(input);
}

void Player::UpdateCamera() {
    // Update camera position to player eye position
    glm::vec3 eyePosition = position + eyeOffset;
    camera.setCamPos(eyePosition);;
}

void Player::Move(const glm::vec3& direction) {
    // Get current movement speed based on state
    float currentSpeed = walkSpeed;

    Logger::Debug("Player Move: direction=(" + 
                  std::to_string(direction.x) + ", " + 
                  std::to_string(direction.y) + ", " + 
                  std::to_string(direction.z) + "), state=" + 
                  std::to_string(static_cast<int>(movementState)));
    
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
    
    // Apply movement
    glm::vec3 movement = direction * currentSpeed * Screen::GetInst().GetDeltaTime();
    
    if (flying || gameMode == GameMode::Spectator) {
        // Free flight movement (spectator noclip)
        position += movement;
    } else {
        // Ground-based movement for survival
        position.x += movement.x;
        position.z += movement.z;
        position.y += movement.y;
    }

    RecalculateCameraPosition();

    // Update velocity for other systems
    velocity = movement / Screen::GetInst().GetDeltaTime();
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
    }
    
    // Normalize movement direction if needed
    if (glm::length(moveDirection) > 1.0f) {
        moveDirection = glm::normalize(moveDirection);
    }
    
    // Apply movement
    if (glm::length(moveDirection) > 0.0f) {
        Move(moveDirection);
    }
}

void Player::ProcessCameraInput(const InputManager& input) {
    // Use keyboard controls for camera movement (arrow keys only)
    
    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;
    
    // Arrow keys for camera rotation
    if (input.IsActionHeld("camera_left")) {
        yawDelta -= input.GetMouseSensitivity() * Screen::GetInst().GetDeltaTime();
    }
    if (input.IsActionHeld("camera_right")) {
        yawDelta += input.GetMouseSensitivity() * Screen::GetInst().GetDeltaTime();
    }
    if (input.IsActionHeld("camera_up")) {
        pitchDelta += input.GetMouseSensitivity() * Screen::GetInst().GetDeltaTime();
    }
    if (input.IsActionHeld("camera_down")) {
        pitchDelta -= input.GetMouseSensitivity() * Screen::GetInst().GetDeltaTime();
    }
    
    // Update camera direction
    camera.setCamDir(camera.yaw += yawDelta, camera.pitch += pitchDelta);
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