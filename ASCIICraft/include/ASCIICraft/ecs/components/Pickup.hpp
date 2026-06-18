#pragma once

struct Pickup {
    float pickupDelay = 0.5f;     // Time before the item can be picked up
    float magnetRadius = 2.0f;    // Player enters this range → item pulls toward player center
    float collectRadius = 0.5f;   // Item reaches this range → transfer to inventory
    uint32_t throwerId = 0;       // Entity ID of the thrower (for delay logic)
};
