#include <ASCIICraft/player/Player.hpp>
#include <ASCIIgL/engine/Logger.hpp>
#include <algorithm>
#include <cmath>

Player::Player(const glm::vec3& startPosition)
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
    , gameMode(GameMode::Creative)
    , movementState(MovementState::Walking)
    , onGround(false)
    , flying(true) // Start flying in creative mode
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

void Player::Update(float deltaTime) {
    // Update movement state
    UpdateMovementState();
    
    // Update camera
    UpdateCamera();
    
    // Update timers
    jumpCooldown = std::max(0.0f, jumpCooldown - deltaTime);
    lastOnGround += deltaTime;
    
    // Reset jump pressed flag
    jumpPressed = false;
}

void Player::HandleInput(const InputManager& input, float deltaTime) {
    ProcessMovementInput(input, deltaTime);
    ProcessCameraInput(input);
}

void Player::UpdateCamera() {
    // Update camera position to player eye position
    glm::vec3 eyePosition = position + eyeOffset;
    camera.setCamPos(eyePosition);
    
    // Recalculate view matrix
    camera.recalculateViewMat();
}

void Player::Move(const glm::vec3& direction, float deltaTime) {
    // Get current movement speed based on state
    float currentSpeed = walkSpeed;
    
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
    glm::vec3 movement = direction * currentSpeed * deltaTime;
    
    if (flying || gameMode == GameMode::Creative) {
        // Free flight movement
        position += movement;
    } else {
        position.x += movement.x;
        position.z += movement.z;
        position.y += movement.y;
    }

    RecalculateCameraPosition();

    // Update velocity for other systems
    velocity = movement / deltaTime;
}

void Player::RecalculateCameraPosition() {
    camera.setCamPos(position + eyeOffset);
}

void Player::SetPosition(const glm::vec3& pos) {
    position = pos;
    UpdateCamera();
    
    Logger::Debug("Player position set to (" + 
                  std::to_string(position.x) + ", " + 
                  std::to_string(position.y) + ", " + 
                  std::to_string(position.z) + ")");
}

void Player::SetGameMode(GameMode mode) {
    gameMode = mode;
    
    // Update movement capabilities based on game mode
    switch (mode) {
        case GameMode::Creative:
            flying = true;
            movementState = MovementState::Flying;
            break;
        case GameMode::Survival:
            flying = false;
            movementState = MovementState::Walking;
            break;
        case GameMode::Spectator:
            flying = true;
            movementState = MovementState::Flying;
            break;
    }
    
    Logger::Info("Player game mode changed to " + std::to_string(static_cast<int>(mode)));
}

void Player::Respawn(const glm::vec3& spawnPosition) {
    SetPosition(spawnPosition);
    velocity = glm::vec3(0.0f);
    
    // Reset player state
    movementState = (gameMode == GameMode::Creative) ? MovementState::Flying : MovementState::Walking;
    flying = (gameMode == GameMode::Creative || gameMode == GameMode::Spectator);
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
    if (flying) {
        movementState = MovementState::Flying;
    } else if (sprinting) {
        movementState = MovementState::Running;
    } else if (sneaking) {
        movementState = MovementState::Sneaking;
    } else if (!onGround) {
        movementState = MovementState::Falling;
    } else {
        movementState = MovementState::Walking;
    }
}

void Player::ProcessMovementInput(const InputManager& input, float deltaTime) {
    glm::vec3 moveDirection(0.0f);
    
    // Get camera directions for movement
    glm::vec3 forward = camera.getCamFront();
    glm::vec3 right = camera.getCamRight();
    
    // Normalize forward and right for horizontal movement
    if (!flying) {
        forward.y = 0.0f;
        right.y = 0.0f;
        forward = glm::normalize(forward);
        right = glm::normalize(right);
    }
    
    // Process WASD movement
    if (input.IsActionPressed("move_forward")) {
        moveDirection += forward;
    }
    if (input.IsActionPressed("move_backward")) {
        moveDirection -= forward;
    }
    if (input.IsActionPressed("move_left")) {
        moveDirection -= right;
    }
    if (input.IsActionPressed("move_right")) {
        moveDirection += right;
    }
    
    // Process vertical movement for flying
    if (flying || gameMode == GameMode::Creative) {
        if (input.IsActionPressed("jump")) {
            moveDirection.y += 1.0f;
        }
        if (input.IsActionPressed("sneak")) {
            moveDirection.y -= 1.0f;
        }
    }
    
    // Normalize movement direction if needed
    if (glm::length(moveDirection) > 1.0f) {
        moveDirection = glm::normalize(moveDirection);
    }
    
    // Apply movement
    if (glm::length(moveDirection) > 0.0f) {
        Move(moveDirection, deltaTime);
    }
}

void Player::ProcessCameraInput(const InputManager& input) {
    // Use keyboard controls for camera movement (arrow keys only)
    
    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;
    
    // Arrow keys for camera rotation
    if (input.IsActionPressed("camera_left")) {
        yawDelta -= input.GetMouseSensitivity() * Screen::GetInstance().GetDeltaTime();
    }
    if (input.IsActionPressed("camera_right")) {
        yawDelta += input.GetMouseSensitivity() * Screen::GetInstance().GetDeltaTime();
    }
    if (input.IsActionPressed("camera_up")) {
        pitchDelta += input.GetMouseSensitivity() * Screen::GetInstance().GetDeltaTime();
    }
    if (input.IsActionPressed("camera_down")) {
        pitchDelta -= input.GetMouseSensitivity() * Screen::GetInstance().GetDeltaTime();
    }
    
    // Update camera direction
    camera.setCamDir(camera.yaw += yawDelta, camera.pitch += pitchDelta);
}

void Player::ProcessActionInput(const InputManager& input, World& world) {
    // Process game mode toggle (for testing - remove later)
    // if (input.IsActionJustPressed("toggle_gamemode")) {
    //     GameMode newMode = (gameMode == GameMode::Creative) ? GameMode::Survival : GameMode::Creative;
    //     SetGameMode(newMode);
    // }
    
    // Block interaction
    if (input.IsActionPressed("break_block")) {
        // Q key - Left click replacement for breaking blocks
        // TODO: Implement block breaking when BreakBlock method is available
        Logger::Debug("Break block action pressed");
    }
    
    if (input.IsActionPressed("place_block")) {
        // R key - Right click replacement for placing blocks
        // TODO: Implement block placing when PlaceBlock method is available
        Logger::Debug("Place block action pressed");
    }
    
    // Inventory toggle
    if (input.IsActionPressed("open_inventory")) {
        Logger::Debug("Inventory toggle pressed");
    }
}