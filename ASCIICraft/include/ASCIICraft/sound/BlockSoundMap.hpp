#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace blockstate {
class BlockStateRegistry;
}

namespace sound {

/// Shared block-type -> sound-family mapping ("step.grass", "step.wood", ...).
/// Used by both StepSFXSystem (footsteps) and MiningSystem (dig/break sounds,
/// which reuse the step sounds like vanilla Minecraft).
/// Stored in registry.ctx(); the map is built lazily on first resolve.
class BlockSoundMap {
public:
    /// Step-sound id for a block type (e.g. "step.grass").
    /// Falls back to name-based heuristics, then "step.stone".
    const std::string& ResolveStepSoundId(uint16_t typeId, const blockstate::BlockStateRegistry& bsr);

private:
    void Build(const blockstate::BlockStateRegistry& bsr);
    void MapType(const blockstate::BlockStateRegistry& bsr, const char* typeName, const char* soundId);

    std::unordered_map<uint16_t, std::string> m_soundByTypeId;
    bool m_built = false;
};

} // namespace sound
