#pragma once

#include <cstdint>

namespace terrain {

/// Position key for the terrain generator's dedup sets (occupied cells, log and leaf
/// positions).
///
/// PRECONDITION: must be injective over the coordinate range one chunk's generation
/// can touch. A collision makes a dedup set report a cell as already used, which
/// silently drops the block that would have gone there.
///
/// KNOWN DEFECT - this implementation is NOT injective. The three fields overlap:
///   x occupies bits 42..73 (truncated at 63)
///   y occupies bits 21..52
///   z occupies bits  0..31
/// so y aliases both x and z. At negative coordinates it is far worse: uint32_t(-1)
/// is 0xFFFFFFFF, which fills bits 0..31 and collides wholesale with y.
///
/// Kept verbatim on purpose. world/tier1_terrain_keys.cpp pins the collisions as
/// should_fail regression tests; the fix (a splitmix64-mixed 3-tuple) lands after
/// those tests exist, at which point they flip to permanent pins.
inline uint64_t BlockKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42) ^
           (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 21) ^
           static_cast<uint64_t>(static_cast<uint32_t>(z));
}

} // namespace terrain
