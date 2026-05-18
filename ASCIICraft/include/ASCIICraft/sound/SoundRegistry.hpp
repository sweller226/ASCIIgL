#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace sound {

/// Maps logical sound ids to one or more file paths (variants picked at random on play).
class SoundRegistry {
public:
    void Register(const std::string& soundId, std::vector<std::string> paths);
    void Register(const std::string& soundId, const std::string& path);

    bool Has(const std::string& soundId) const;

    /// Empty string if \p soundId is unknown or has no paths.
    std::string PickRandomPath(const std::string& soundId) const;

    const std::vector<std::string>* TryGetPaths(const std::string& soundId) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> m_entries;
};

void RegisterDefaultSounds(SoundRegistry& registry);

} // namespace sound
