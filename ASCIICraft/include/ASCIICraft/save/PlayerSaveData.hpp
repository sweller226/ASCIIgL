#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include <glm/vec3.hpp>

#include <ASCIICraft/ecs/factories/PlayerSpawnState.hpp>

namespace save {

/// Bump only for a breaking change. Adding a field is not breaking: unknown keys are
/// ignored on read, and a missing key falls back to its default.
inline constexpr int kPlayerDataVersion = 1;

/// Mirrors Camera3D::setCamDir's default clamp. Re-applied on load because that clamp
/// is comparison-based and therefore NaN-blind - see PlayerDataJson.hpp.
inline constexpr float kPitchClampDegrees = 89.9f;

/// Sanity bound for a corrupt file. The world is 1024 blocks across, so anything past
/// this is garbage rather than a coordinate someone legitimately reached.
inline constexpr float kMaxAbsCoordinate = 1.0e6f;

struct PlayerSaveData {
    glm::vec3 position{0.0f};
    float yawDegrees   = ecs::factories::kDefaultSpawnYawDegrees;
    float pitchDegrees = ecs::factories::kDefaultSpawnPitchDegrees;
    GameMode gameMode  = GameMode::Survival;
};

/// Stored as a string rather than the enum's integer so the file stays hand-editable
/// and survives any future reordering of GameMode.
inline const char* GameModeToString(GameMode mode) {
    switch (mode) {
        case GameMode::Creative:  return "creative";
        case GameMode::Spectator: return "spectator";
        case GameMode::Survival:  break;
    }
    return "survival";
}

inline bool GameModeFromString(const std::string& text, GameMode& out) {
    if (text == "survival")  { out = GameMode::Survival;  return true; }
    if (text == "creative")  { out = GameMode::Creative;  return true; }
    if (text == "spectator") { out = GameMode::Spectator; return true; }
    return false;
}

/// True when every field is a usable number. Guards the write path: capturing a NaN
/// position from a wedged physics step and then dumping it would overwrite a good save
/// with one that cannot be loaded back (nlohmann serialises non-finite floats as null).
inline bool IsFinite(const PlayerSaveData& data) {
    return std::isfinite(data.position.x) && std::isfinite(data.position.y) &&
           std::isfinite(data.position.z) && std::isfinite(data.yawDegrees) &&
           std::isfinite(data.pitchDegrees);
}

/// Wraps yaw into [-180, 180). Cosmetic - the camera treats all coterminal angles
/// alike - but it keeps a hand-inspected file readable after a lot of spinning.
inline float NormalizeYawDegrees(float yaw) {
    float wrapped = std::fmod(yaw + 180.0f, 360.0f);
    if (wrapped < 0.0f) {
        wrapped += 360.0f;
    }
    return wrapped - 180.0f;
}

inline float ClampPitchDegrees(float pitch) {
    return std::clamp(pitch, -kPitchClampDegrees, kPitchClampDegrees);
}

inline ecs::factories::PlayerSpawnState ToSpawnState(const PlayerSaveData& data) {
    ecs::factories::PlayerSpawnState spawn;
    spawn.position     = data.position;
    spawn.yawDegrees   = data.yawDegrees;
    spawn.pitchDegrees = data.pitchDegrees;
    spawn.mode         = data.gameMode;
    return spawn;
}

} // namespace save
