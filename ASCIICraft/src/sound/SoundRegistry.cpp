#include <ASCIICraft/sound/SoundRegistry.hpp>

#include <ASCIICraft/util/RNG.hpp>

namespace sound {

void SoundRegistry::Register(const std::string& soundId, std::vector<std::string> paths) {
    if (paths.empty()) {
        return;
    }
    m_entries[soundId] = std::move(paths);
}

void SoundRegistry::Register(const std::string& soundId, const std::string& path) {
    Register(soundId, std::vector<std::string>{path});
}

bool SoundRegistry::Has(const std::string& soundId) const {
    const auto it = m_entries.find(soundId);
    return it != m_entries.end() && !it->second.empty();
}

const std::vector<std::string>* SoundRegistry::TryGetPaths(const std::string& soundId) const {
    const auto it = m_entries.find(soundId);
    if (it == m_entries.end() || it->second.empty()) {
        return nullptr;
    }
    return &it->second;
}

std::string SoundRegistry::PickRandomPath(const std::string& soundId) const {
    const std::vector<std::string>* paths = TryGetPaths(soundId);
    if (!paths) {
        return {};
    }
    if (paths->size() == 1) {
        return (*paths)[0];
    }

    static util::RNG rng;
    return (*paths)[rng.NextInt(0, static_cast<int>(paths->size()) - 1)];
}

} // namespace sound
