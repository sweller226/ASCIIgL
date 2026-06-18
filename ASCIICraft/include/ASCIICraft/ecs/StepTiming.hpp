#pragma once

#include <ASCIICraft/ecs/components/PlayerController.hpp>

namespace ecs {

struct StepTiming {
    static constexpr float MIN_STEP_SPEED      = 0.15f;
    static constexpr float STEP_COOLDOWN       = 0.10f;
    static constexpr float WALK_STEP_DISTANCE  = 1.6f;
    static constexpr float RUN_STEP_DISTANCE   = 1.4f;
    static constexpr float SNEAK_STEP_DISTANCE = 2.0f;

    static float GetStepDistance(const components::PlayerController* ctrl) {
        if (!ctrl) {
            return WALK_STEP_DISTANCE;
        }

        switch (ctrl->movementState) {
            case MovementState::Running:
                return RUN_STEP_DISTANCE;
            case MovementState::Sneaking:
                return SNEAK_STEP_DISTANCE;
            default:
                return WALK_STEP_DISTANCE;
        }
    }
};

} // namespace ecs
