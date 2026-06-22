#pragma once

namespace ecs::components {

// Minecraft-style view bobbing state (Entity.distanceWalkedModified + EntityPlayer cameraYaw/Pitch).
struct ViewBobbing {
    float distanceWalkedModified     = 0.0f;
    float prevDistanceWalkedModified = 0.0f;

    float cameraYaw     = 0.0f;
    float prevCameraYaw = 0.0f;

    float cameraPitch     = 0.0f;
    float prevCameraPitch = 0.0f;

    float distanceWalkedOnStepModified = 0.0f;
    int   nextStepDistance             = 0;

    bool enabled = true;

    // Scales all bob translation/rotation output.
    float intensity = 0.7;

    static constexpr float MAX_CAMERA_YAW         = 0.1f;
    static constexpr float CAMERA_YAW_LERP        = 0.4f;
    static constexpr float CAMERA_PITCH_LERP      = 0.5f;
    static constexpr float VERTICAL_MOTION_FACTOR = 0.1f;
    static constexpr float PITCH_MOTION_SCALE     = 2.0f;
    static constexpr float WALK_DISTANCE_SCALE    = 0.6f;
};

} // namespace ecs::components
