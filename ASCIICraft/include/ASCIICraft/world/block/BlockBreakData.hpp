#pragma once

#include <cstdint>

/// Tool classes used to match held tools (items) against blocks for mining
/// speed and harvest gating. Shared by block break data (BlockType) and item
/// definitions (ToolProperty), so it lives in its own header to avoid a
/// dependency between the block registry and item components.
enum class ToolClass : uint8_t {
    None,
    Pickaxe,
    Axe,
    Shovel,
    Sword
};

namespace blockstate { class BlockStateRegistry; }

namespace blockbreak {

/// Applies hardness / preferred tool / required harvest level to all
/// registered block types (keyed by type name; see the table in
/// BlockBreakData.cpp). Call once after all block types are registered.
/// Types missing from the table keep BlockType defaults (hardness 1, no tool).
void ApplyBlockBreakData(blockstate::BlockStateRegistry& bsr);

} // namespace blockbreak
