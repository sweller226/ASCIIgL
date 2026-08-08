#pragma once

#include <cstdint>

namespace blockstate {
class BlockStateRegistry;
}

/// Read-path remap from v1 on-disk numeric stateIds to current registry stateIds.
///
/// v1 chunk and meta blobs stored raw numeric stateIds, so their meaning depends on
/// the block registration order at the time they were written. v2 blobs store block
/// names and properties instead, and are unaffected by this.
///
/// Nothing is rewritten in place: a v1 blob is remapped when read, and the next save
/// of that chunk writes v2. A world therefore converts itself lazily, after which
/// Remap() is never called again.
namespace v1_state_id {

/// Rebuild the remap table from the live registry. Call once after all types are
/// registered. The table is process-global: only one registry per process may build
/// it, or migration breaks for the others.
void BuildRemapTable(const blockstate::BlockStateRegistry& bsr);

/// Map a v1 on-disk stateId to the current registry stateId. Unknown/OOB → air.
uint32_t Remap(uint32_t v1StateId);

} // namespace v1_state_id
