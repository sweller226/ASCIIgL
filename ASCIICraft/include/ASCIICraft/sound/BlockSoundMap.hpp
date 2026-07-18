#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace blockstate {
class BlockStateRegistry;
}

namespace sound {

/// Shared block-type -> sound-family mapping ("grass", "wood", "stone", ...).
/// Families are prefixed to form concrete sound ids:
/// - "step.<family>": footsteps (StepSFXSystem) and the mining hit loop (MiningSystem)
/// - "dig.<family>":  block break sound (MiningSystem)
/// Stored in registry.ctx(); the map is built lazily on first resolve.
class BlockSoundMap {
public:
    /// e.g. "step.grass". Falls back to name-based heuristics, then "stone".
    std::string ResolveStepSoundId(uint16_t typeId, const blockstate::BlockStateRegistry& bsr);

    /// e.g. "dig.grass". Same family as the step sound.
    std::string ResolveDigSoundId(uint16_t typeId, const blockstate::BlockStateRegistry& bsr);

private:
    const std::string& ResolveFamily(uint16_t typeId, const blockstate::BlockStateRegistry& bsr);

    void Build(const blockstate::BlockStateRegistry& bsr);
    void MapType(const blockstate::BlockStateRegistry& bsr, const char* typeName, const char* family);

    std::unordered_map<uint16_t, std::string> m_familyByTypeId;
    bool m_built = false;
};

} // namespace sound
