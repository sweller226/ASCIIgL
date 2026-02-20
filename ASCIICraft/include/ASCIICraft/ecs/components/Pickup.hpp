#pragma once

struct Pickup {
    float pickupDelay = 0.0f;   // Time before the item can be picked up
    float pickupRadius = 1.0f;  // How close a player must be
    uint32_t throwerId = 0;     // Entity ID of the thrower (for delay logic)
};
