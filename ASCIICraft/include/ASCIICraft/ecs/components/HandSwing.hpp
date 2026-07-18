#pragma once

namespace ecs::components {

/// First-person held-item swing state. Triggered by MiningSystem (looping while
/// mining) and PlacingSystem (single swing); advanced by ViewBobbingSystem;
/// applied as a transform by HeldItemRenderSystem.
struct HandSwing {
    static constexpr float kSwingDurationSeconds = 0.25f;

    bool swinging = false;
    float progress = 0.0f;  // 0..1 over one swing

    /// Start a swing if one isn't already running. Calling every frame while
    /// mining yields a continuous loop (each completed swing restarts).
    void Trigger() {
        if (!swinging) {
            swinging = true;
            progress = 0.0f;
        }
    }
};

} // namespace ecs::components
