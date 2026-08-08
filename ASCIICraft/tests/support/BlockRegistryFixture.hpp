#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace blockstate { class BlockStateRegistry; }
namespace blockmodels { class BlockModelLibrary; }

namespace testsupport {

/// Commonly needed state ids, resolved once.
struct VanillaIds {
    uint32_t air = 0;
    uint32_t dirt = 0;
    uint32_t grass = 0;
    uint32_t stone = 0;
    uint32_t water = 0;
    uint32_t oakLog = 0;
    uint32_t oakLeaves = 0;
    uint32_t poppy = 0;
    uint32_t dandelion = 0;
    uint32_t fern = 0;
    uint32_t tallGrass = 0;
    uint32_t glass = 0;
};

/// One registry with all vanilla blocks, built on first use and shared for the rest
/// of the process.
///
/// Shared rather than per-test for two reasons. Registration reads ~40 JSON
/// blockstates plus their models, which would dominate runtime if repeated. More
/// importantly it builds the v1 remap table, which is process-global mutable state -
/// a second registry building it would corrupt migration for the first. Any test
/// needing its own registry must pass VanillaRegistrationOptions::buildLegacyRemapTable
/// = false.
///
/// Thread-safe on first call (function-local static). The returned objects are not
/// mutated afterwards, so concurrent reads are safe.
entt::registry& SharedRegistry();

const blockstate::BlockStateRegistry& SharedBlocks();
const blockmodels::BlockModelLibrary& SharedModels();
const VanillaIds& Ids();

} // namespace testsupport
