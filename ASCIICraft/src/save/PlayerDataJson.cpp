#include <ASCIICraft/save/PlayerDataJson.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <cmath>

namespace save {

namespace {

/// Structural rejection helper - keeps the failure strings uniform.
jsonutil::LoadResult<PlayerSaveData> Reject(const std::string& debugName, const std::string& why) {
    return jsonutil::Fail<PlayerSaveData>(debugName + ": " + why);
}

/// Reads an optional finite angle. Absent -> default (soft). Present but not a finite
/// number -> structural failure, because a garbage angle means a garbage file.
bool ReadOptionalAngle(const nlohmann::json& j, const char* key, float& inOut, bool& structurallyBad) {
    const auto it = j.find(key);
    if (it == j.end()) {
        return false;   // absent: keep the default
    }
    if (!it->is_number()) {
        structurallyBad = true;
        return false;
    }
    const float value = it->get<float>();
    if (!std::isfinite(value)) {
        structurallyBad = true;
        return false;
    }
    inOut = value;
    return true;
}

} // namespace

nlohmann::json PlayerDataToJson(const PlayerSaveData& data) {
    return nlohmann::json{
        {"version",  kPlayerDataVersion},
        {"position", {data.position.x, data.position.y, data.position.z}},
        {"yaw",      data.yawDegrees},
        {"pitch",    data.pitchDegrees},
        {"gamemode", GameModeToString(data.gameMode)},
    };
}

jsonutil::LoadResult<PlayerSaveData> PlayerDataFromJson(const nlohmann::json& j,
                                                        const std::string& debugName) {
    if (!j.is_object()) {
        return Reject(debugName, "top level must be an object");
    }

    // --- version (structural) ---
    const auto versionIt = j.find("version");
    if (versionIt == j.end() || !versionIt->is_number_integer()) {
        return Reject(debugName, "missing or non-integer 'version'");
    }
    const int version = versionIt->get<int>();
    if (version != kPlayerDataVersion) {
        return Reject(debugName, "unsupported 'version' " + std::to_string(version) +
                                 " (this build reads " + std::to_string(kPlayerDataVersion) +
                                 "); it was probably written by a newer build");
    }

    PlayerSaveData out;

    // --- position (structural) ---
    const auto positionIt = j.find("position");
    if (positionIt == j.end()) {
        return Reject(debugName, "missing 'position'");
    }
    const auto position = jsonutil::ParseFloatArray<3>(*positionIt, debugName, "position");
    if (!position.Ok()) {
        return jsonutil::Fail<PlayerSaveData>(*position.error);
    }
    for (const float component : *position.value) {
        // The gate that matters. NaN or a wild coordinate must never reach the ECS.
        if (!std::isfinite(component) || std::fabs(component) > kMaxAbsCoordinate) {
            return Reject(debugName, "'position' contains a non-finite or out-of-range value");
        }
    }
    out.position = glm::vec3((*position.value)[0], (*position.value)[1], (*position.value)[2]);

    // --- yaw / pitch (absent is soft, malformed is structural) ---
    bool anglesBad = false;
    ReadOptionalAngle(j, "yaw", out.yawDegrees, anglesBad);
    ReadOptionalAngle(j, "pitch", out.pitchDegrees, anglesBad);
    if (anglesBad) {
        return Reject(debugName, "'yaw' or 'pitch' is not a finite number");
    }

    out.yawDegrees = NormalizeYawDegrees(out.yawDegrees);

    const float clampedPitch = ClampPitchDegrees(out.pitchDegrees);
    if (clampedPitch != out.pitchDegrees) {
        ASCIIgL::Logger::Warningf("%s: 'pitch' %.2f is out of range; clamping to %.2f.",
                                  debugName.c_str(), out.pitchDegrees, clampedPitch);
        out.pitchDegrees = clampedPitch;
    }

    // --- gamemode (soft) ---
    const auto modeIt = j.find("gamemode");
    if (modeIt == j.end()) {
        ASCIIgL::Logger::Warningf("%s: no 'gamemode'; defaulting to %s.",
                                  debugName.c_str(), GameModeToString(out.gameMode));
    } else if (!modeIt->is_string() || !GameModeFromString(modeIt->get<std::string>(), out.gameMode)) {
        ASCIIgL::Logger::Warningf("%s: unrecognised 'gamemode'; defaulting to %s.",
                                  debugName.c_str(), GameModeToString(out.gameMode));
    }

    return jsonutil::LoadResult<PlayerSaveData>::Success(out);
}

jsonutil::LoadResult<PlayerSaveData> ParsePlayerData(const std::string& text,
                                                     const std::string& debugName) {
    const auto parsed = jsonutil::ParseJson(text, debugName);
    if (!parsed.Ok()) {
        return jsonutil::Fail<PlayerSaveData>(*parsed.error);
    }
    return PlayerDataFromJson(*parsed.value, debugName);
}

} // namespace save
