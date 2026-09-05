#pragma once

#include <glm/vec3.hpp>

#include <ASCIICraft/ecs/components/PlayerMode.hpp>

namespace ecs::factories {

/// -90 degrees yaw / 0 pitch reproduces the historical setCamDir({0, 0, -1}):
/// Camera3D::setCamDir(vec3) computes yaw = degrees(atan2(dir.z, dir.x)), which is
/// -90 for -Z. Spelling it out here keeps the default facing identical to what the
/// game shipped before player state was persisted.
inline constexpr float kDefaultSpawnYawDegrees   = -90.0f;
inline constexpr float kDefaultSpawnPitchDegrees = 0.0f;

/// Everything PlayerFactory needs to place a player, whether that is a fresh spawn or
/// a restored save. Kept in its own header so the save layer can depend on the spawn
/// contract without pulling in PlayerFactory.hpp's component includes.
struct PlayerSpawnState {
    glm::vec3 position{0.0f};
    float yawDegrees   = kDefaultSpawnYawDegrees;
    float pitchDegrees = kDefaultSpawnPitchDegrees;
    GameMode mode      = GameMode::Survival;
};

} // namespace ecs::factories
