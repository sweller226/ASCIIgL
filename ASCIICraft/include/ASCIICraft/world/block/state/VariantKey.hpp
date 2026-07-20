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

/// Parses a \ref BuildVariantKey string into property name/value pairs. Invalid segments are skipped.
std::unordered_map<std::string, std::string> ParseVariantKey(const std::string& key);

/// Stable on-disk identity for a block state (type name + variant key props).
struct SerializedStateIdentity {
    std::string name;
    std::string props;
};

/// Encode a live stateId for persistence. Invalid IDs become air.
SerializedStateIdentity SerializeState(const BlockStateRegistry& bsr, uint32_t stateId);

/// Resolve a persisted name + props key to a current stateId. Unknown names become air.
uint32_t ResolveStateFromSerialized(
    const BlockStateRegistry& bsr,
    const std::string& name,
    const std::string& propsKey
);

/// If any two states of \p typeId produce the same \ref BuildVariantKey string, logs an error
/// (including \p context) and asserts in debug builds. No-op for types with zero or one state.
void AssertUniqueVariantKeysPerType(
    const BlockStateRegistry& bsr,
    uint16_t typeId,
    const char* context = nullptr
);

} // namespace blockstate
