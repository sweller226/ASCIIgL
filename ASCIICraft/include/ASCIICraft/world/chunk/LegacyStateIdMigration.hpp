#pragma once

#include <cstdint>

namespace blockstate {
class BlockStateRegistry;
}

/// Builds and queries the v1 numeric stateId → current stateId remap table.
/// The legacy order matches pre-stone registration in Game::InitializeBlockStates.
namespace legacy_state_id {

/// Rebuild the remap table from the live registry. Call once after all types are registered.
void BuildRemapTable(const blockstate::BlockStateRegistry& bsr);

/// Map a v1 on-disk stateId to the current registry stateId. Unknown/OOB → air.
uint32_t RemapLegacyStateId(uint32_t legacyStateId);

} // namespace legacy_state_id
