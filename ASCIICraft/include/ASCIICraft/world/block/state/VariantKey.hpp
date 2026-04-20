#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace blockstate {

class BlockStateRegistry;

/// Builds a Minecraft 1.8.9-style variant lookup key for \p stateId: properties in
/// \ref BlockType::properties registration order, joined as \c name=value with \c ',' (no spaces).
/// Types with no properties yield an empty string (matches many vanilla default variants).
std::string BuildVariantKey(const BlockStateRegistry& bsr, uint32_t stateId);

/// If any two states of \p typeId produce the same \ref BuildVariantKey string, logs an error
/// (including \p context) and asserts in debug builds. No-op for types with zero or one state.
void AssertUniqueVariantKeysPerType(
    const BlockStateRegistry& bsr,
    uint16_t typeId,
    const char* context = nullptr
);

} // namespace blockstate
