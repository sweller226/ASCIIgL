#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>

#include <ASCIICraft/world/blockstate/BlockType.hpp>
#include <ASCIICraft/world/blockstate/BlockState.hpp>
#include <ASCIICraft/world/blockstate/BlockProperty.hpp>

namespace blockstate {

/// Central registry for all block types and their flattened blockstates.
/// Stored in registry.ctx() for global access.
class BlockStateRegistry {
public:
    /// Air is always type 0, state 0.
    static constexpr uint32_t AIR_STATE_ID = 0;

    // ========================================================================
    // Registration
    // ========================================================================

    /// Register a new block type with named properties.
    /// Returns the assigned typeId.
    uint16_t RegisterType(
        const std::string& name,
        std::vector<BlockProperty> properties = {},
        bool hasBlockEntity = false
    );

    /// Set derived data (textures, solidity, etc.) for all states of a type.
    /// The callback is invoked once per state, with the state pre-populated
    /// with its property values so the callback can branch on them.
    using DerivedCallback = std::function<void(BlockState&)>;
    void SetDerivedData(uint16_t typeId, DerivedCallback callback);

    // ========================================================================
    // Lookup (all O(1))
    // ========================================================================

    const BlockState& GetState(uint32_t stateId) const;
    const BlockType&  GetType(uint16_t typeId) const;

    uint16_t GetTypeId(const std::string& name) const;
    uint32_t GetDefaultState(uint16_t typeId) const;
    uint32_t GetDefaultState(const std::string& name) const;

    /// Get the typeId from a stateId (O(1) via the state's stored typeId).
    uint16_t GetTypeIdFromState(uint32_t stateId) const;

    // ========================================================================
    // Property Mutation (returns NEW state ID, never mutates)
    // ========================================================================

    /// Returns a new stateId with one property changed.
    uint32_t WithProperty(
        uint32_t stateId,
        const std::string& propName,
        const std::string& value
    ) const;

    /// Resolve an explicit property map to a state ID.
    uint32_t Resolve(
        uint16_t typeId,
        const std::unordered_map<std::string, std::string>& props
    ) const;

    // ========================================================================
    // Queries
    // ========================================================================

    uint32_t GetTotalStateCount() const { return static_cast<uint32_t>(states.size()); }
    uint16_t GetTotalTypeCount()  const { return static_cast<uint16_t>(types.size()); }

    /// Check if a stateId is valid.
    bool IsValidState(uint32_t stateId) const { return stateId < states.size(); }

    /// Get a property value string for a given state.
    const std::string& GetPropertyValue(uint32_t stateId, const std::string& propName) const;

private:
    std::vector<BlockType>  types;                          // indexed by typeId
    std::vector<BlockState> states;                         // indexed by stateId (dense)
    std::unordered_map<std::string, uint16_t> nameToType;

    // Flattening helpers
    uint32_t ComputePropertyOffset(
        const BlockType& type,
        const std::vector<uint8_t>& propertyValues
    ) const;

    int FindPropertyIndex(const BlockType& type, const std::string& propName) const;
    int FindValueIndex(const BlockProperty& prop, const std::string& value) const;

    void GenerateStatesForType(BlockType& type);
};

} // namespace blockstate
