#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include <ASCIICraft/save/PlayerSaveData.hpp>
#include <ASCIICraft/util/JsonUtil.hpp>

/// Serialisation for player_data.json. No filesystem access lives here - that is
/// PlayerDataStore's job - so every branch below is directly testable.
///
/// On-disk shape:
///   { "version": 1, "position": [12.5, 90.0, -33.25],
///     "yaw": -90.0, "pitch": 12.5, "gamemode": "survival" }
///
/// Validation is deliberately two-tier, so a corrupt file degrades the way a corrupt
/// chunk does - to a regenerated default - rather than bricking the game:
///
///   Structural error -> the whole file is rejected and the caller falls back to the
///   world spawn point. Covers: not an object; a missing, non-integer, or mismatched
///   "version"; a missing or malformed "position"; and any non-finite or absurd
///   coordinate. That last one is the critical gate - a NaN reaching Transform::position
///   poisons chunk-streaming maths and the view matrix, and Camera3D's own pitch clamp
///   is comparison-based, so it lets NaN straight through.
///
///   Soft error -> that one field falls back to its default, a Warning is logged, and
///   the rest of the file is honoured. Covers: absent yaw/pitch; an absent or
///   unrecognised gamemode; and an out-of-range pitch, which is clamped here before it
///   can reach setCamDir. A typo in one field should not cost the player their position.
///
/// Unknown keys are ignored, which is the forward-compatible half of "version".
namespace save {

nlohmann::json PlayerDataToJson(const PlayerSaveData& data);

jsonutil::LoadResult<PlayerSaveData> PlayerDataFromJson(const nlohmann::json& j,
                                                        const std::string& debugName);

jsonutil::LoadResult<PlayerSaveData> ParsePlayerData(const std::string& text,
                                                     const std::string& debugName);

} // namespace save
