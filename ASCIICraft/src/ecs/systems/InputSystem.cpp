#include <ASCIICraft/ecs/systems/InputSystem.hpp>

void InputSystem::ProcessMovementInput() {
    const auto input = ASCIIgL::InputManager::GetInst();
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