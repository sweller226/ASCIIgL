#pragma once

#include <string>

#include <entt/entt.hpp>

namespace blockmodels { class BlockModelLibrary; }

namespace blockstate {

class BlockStateRegistry;

/// Options for vanilla block registration.
struct VanillaRegistrationOptions {
    /// Root containing blockstates/ and models/. Relative paths resolve against the
    /// process CWD, matching how the game has always loaded them.
    std::string assetRoot = "res";

    /// Apply block break (hardness/tool) data after registration.
    bool applyBlockBreakData = true;

    /// Build the global legacy state-id remap table used to migrate v1 chunk blobs.
    ///
    /// WARNING: the remap table is process-global. Exactly one registry per process
    /// may build it - a second registry doing so corrupts migration for the first.
    /// Tests that construct an additional registry must pass false here.
    bool buildLegacyRemapTable = true;

    /// Assert that every type's variant keys are unique. Cheap, on by default.
    bool assertUniqueVariantKeys = true;
};

/// Registers every vanilla block type, its derived render data, and its
/// JSON-backed models into the supplied registry and model library.
///
/// GPU-free: touches no Renderer, Screen, TextureLibrary, or Mesh, so this is
/// safe to call from a headless test binary.
///
/// Registration order determines state id assignment. Nothing persisted depends on
/// those numeric ids - v2 blobs store names and properties, and v1 blobs are remapped
/// through v1_state_id::Remap, whose table resolves its fixed name list against the
/// live registry. The hard constraints are narrower:
///   - minecraft:air must stay typeId 0 / stateId 0 (BlockStateRegistry::AIR_STATE_ID)
///   - every name in v1_state_id's kV1TypeOrder must still be registered
/// Reordering is otherwise safe, but there is no reason to, so don't.
void RegisterVanillaBlocks(BlockStateRegistry& bsr,
                           blockmodels::BlockModelLibrary& modelLibrary,
                           const VanillaRegistrationOptions& opts = {});

/// Convenience overload: emplaces a BlockStateRegistry and BlockModelLibrary into
/// registry.ctx(), then calls RegisterVanillaBlocks on them.
void RegisterVanillaBlocksInContext(entt::registry& registry,
                                    const VanillaRegistrationOptions& opts = {});

} // namespace blockstate
