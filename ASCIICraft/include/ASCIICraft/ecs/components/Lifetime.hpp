#pragma once

struct Lifetime {
    float ageSeconds = 0.0f;        // How long this entity has existed
    float maxLifetimeSeconds = 0.0f; // Max age before despawn (0 = never despawn)
    bool shouldDespawn = false;     // Flag for forced despawn (e.g., void, lava)
};
