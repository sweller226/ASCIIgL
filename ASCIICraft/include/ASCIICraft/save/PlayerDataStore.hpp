#pragma once

#include <filesystem>
#include <optional>

#include <ASCIICraft/save/PlayerSaveData.hpp>

namespace save {

/// Reads and writes player_data.json. All filesystem access for player state lives
/// here; parsing lives in PlayerDataJson. Neither ever throws - a save that cannot be
/// read must cost the player their position, not their session.
class PlayerDataStore {
public:
    explicit PlayerDataStore(std::filesystem::path filePath);

    const std::filesystem::path& FilePath() const { return filePath_; }

    /// True when a file is present, regardless of whether it parses. Lets a caller tell
    /// "first launch" apart from "corrupt file" after Load() returns nullopt.
    bool Exists() const;

    /// nullopt when the file is absent (first launch, logged at Debug) or cannot be
    /// read or parsed (logged at Warning).
    std::optional<PlayerSaveData> Load() const;

    /// Writes "<file>.tmp" and renames it over the target, so a crash or a kill during
    /// the write leaves the previous save intact rather than a truncated file. Refuses
    /// to write non-finite data - see IsFinite in PlayerSaveData.hpp.
    bool Save(const PlayerSaveData& data) const;

private:
    std::filesystem::path filePath_;
};

} // namespace save
