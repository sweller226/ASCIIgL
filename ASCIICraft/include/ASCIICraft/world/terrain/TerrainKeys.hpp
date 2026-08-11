#pragma once

#include <cstdint>

namespace terrain {

/// Position key for the terrain generator's dedup sets (occupied cells, log and leaf
/// positions).
///
/// A collision makes a dedup set report a cell as already used, so the block that
/// would have gone there is silently dropped - a hole in a tree canopy, or missing
/// ground cover.
///
/// The three fields overlap: x occupies bits 42..63 (truncated), y bits 21..52, z bits
/// 0..31, combined with XOR. That looks alarming, and it was originally blamed for
/// missing flowers and canopy holes.
///
/// It is not a live defect. What decides a collision is which bits *vary* across the
/// positions hashed together, and across any footprint this generator can reach those
/// ranges are disjoint:
///
///     x in [-1040,-1008)  ->  x<<42 varies in bits 42..52
///     y in [0, 256)       ->  y<<21 varies in bits 21..28
///     z in [-1040,-1008)  ->  z     varies in bits  0..10
///
/// world/tier1_terrain_keys.cpp verifies injectivity at the origin, at negative
/// coordinates, across the sign boundary, over a full-height column, and at the
/// +-16384 world-extent corners. Triggering an actual collision needs a coordinate
/// around 2^21, roughly 100x the world extent.
///
/// Deliberately left as-is. Replacing it with a mixed hash was tried and reverted:
/// it fixed no observable bug, cost ~5x the arithmetic on a path called thousands of
/// times per chunk, and traded exact-in-range behaviour for a probabilistic one.
///
/// IF WORLD HEIGHT OR EXTENT EVER EXCEEDS 2^21, this becomes a real defect. The
/// injectivity tests are scoped to the current bounds and will not warn you; widen
/// them alongside the world size.
inline uint64_t BlockKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42) ^
           (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 21) ^
           static_cast<uint64_t>(static_cast<uint32_t>(z));
}

} // namespace terrain
