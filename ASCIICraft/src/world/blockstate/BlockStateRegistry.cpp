#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <cassert>
#include <numeric>

namespace blockstate {

// ============================================================================
// Registration
// ============================================================================

uint16_t BlockStateRegistry::RegisterType(
    const std::string& name,
    std::vector<BlockProperty> properties,
    bool hasBlockEntity
) {
    // Prevent duplicate registration
    if (nameToType.count(name)) {
        ASCIIgL::Logger::Warning("BlockStateRegistry: type '" + name + "' already registered");
        return nameToType.at(name);
    }

    uint16_t typeId = static_cast<uint16_t>(types.size());

    BlockType type;
    type.typeId = typeId;
    type.name = name;
    type.properties = std::move(properties);
    type.hasBlockEntity = hasBlockEntity;
    type.baseStateId = static_cast<uint32_t>(states.size());

    // Compute state count = product of all property cardinalities (1 if no properties)
    type.stateCount = 1;
    for (const auto& prop : type.properties) {
        type.stateCount *= prop.Cardinality();
    }

    // Compute default state offset from default property indices
    std::vector<uint8_t> defaultValues;
    for (const auto& prop : type.properties) {
        defaultValues.push_back(prop.defaultIndex);
    }
    type.defaultStateId = type.baseStateId + ComputePropertyOffset(type, defaultValues);

    types.push_back(type);
    nameToType[name] = typeId;

    // Generate all flattened states for this type
    GenerateStatesForType(types.back());

    ASCIIgL::Logger::Debug("BlockStateRegistry: registered '" + name +
        "' (typeId=" + std::to_string(typeId) +
        ", states=" + std::to_string(type.stateCount) +
        ", base=" + std::to_string(type.baseStateId) + ")");

    return typeId;
}

void BlockStateRegistry::SetDerivedData(uint16_t typeId, DerivedCallback callback) {
    assert(typeId < types.size() && "Invalid typeId");
    const auto& type = types[typeId];

    for (uint32_t i = 0; i < type.stateCount; ++i) {
        callback(states[type.baseStateId + i]);
    }
}

// ============================================================================
// Lookup
// ============================================================================

const BlockState& BlockStateRegistry::GetState(uint32_t stateId) const {
    assert(stateId < states.size() && "Invalid stateId");
    return states[stateId];
}

const BlockType& BlockStateRegistry::GetType(uint16_t typeId) const {
    assert(typeId < types.size() && "Invalid typeId");
    return types[typeId];
}

uint16_t BlockStateRegistry::GetTypeId(const std::string& name) const {
    auto it = nameToType.find(name);
    if (it == nameToType.end()) {
        ASCIIgL::Logger::Error("BlockStateRegistry: unknown type '" + name + "'");
        return 0; // Fall back to air
    }
    return it->second;
}

uint32_t BlockStateRegistry::GetDefaultState(uint16_t typeId) const {
    assert(typeId < types.size() && "Invalid typeId");
    return types[typeId].defaultStateId;
}

uint32_t BlockStateRegistry::GetDefaultState(const std::string& name) const {
    return GetDefaultState(GetTypeId(name));
}

uint16_t BlockStateRegistry::GetTypeIdFromState(uint32_t stateId) const {
    assert(stateId < states.size() && "Invalid stateId");
    return states[stateId].typeId;
}

// ============================================================================
// Property Mutation
// ============================================================================

uint32_t BlockStateRegistry::WithProperty(
    uint32_t stateId,
    const std::string& propName,
    const std::string& value
) const {
    assert(stateId < states.size() && "Invalid stateId");

    const auto& state = states[stateId];
    const auto& type = types[state.typeId];

    int propIdx = FindPropertyIndex(type, propName);
    if (propIdx < 0) {
        ASCIIgL::Logger::Warning("BlockStateRegistry: property '" + propName +
            "' not found on type '" + type.name + "'");
        return stateId; // Return unchanged
    }

    int valIdx = FindValueIndex(type.properties[propIdx], value);
    if (valIdx < 0) {
        ASCIIgL::Logger::Warning("BlockStateRegistry: value '" + value +
            "' not valid for property '" + propName + "'");
        return stateId;
    }

    // Copy current property values and replace the target
    std::vector<uint8_t> newValues = state.propertyValues;
    newValues[propIdx] = static_cast<uint8_t>(valIdx);

    return type.baseStateId + ComputePropertyOffset(type, newValues);
}

uint32_t BlockStateRegistry::Resolve(
    uint16_t typeId,
    const std::unordered_map<std::string, std::string>& props
) const {
    assert(typeId < types.size() && "Invalid typeId");
    const auto& type = types[typeId];

    // Start with defaults
    std::vector<uint8_t> values;
    values.reserve(type.properties.size());
    for (const auto& prop : type.properties) {
        values.push_back(prop.defaultIndex);
    }

    // Override with provided properties
    for (const auto& [propName, value] : props) {
        int propIdx = FindPropertyIndex(type, propName);
        if (propIdx < 0) continue;

        int valIdx = FindValueIndex(type.properties[propIdx], value);
        if (valIdx < 0) continue;

        values[propIdx] = static_cast<uint8_t>(valIdx);
    }

    return type.baseStateId + ComputePropertyOffset(type, values);
}

const std::string& BlockStateRegistry::GetPropertyValue(
    uint32_t stateId,
    const std::string& propName
) const {
    static const std::string empty;
    assert(stateId < states.size() && "Invalid stateId");

    const auto& state = states[stateId];
    const auto& type = types[state.typeId];

    int propIdx = FindPropertyIndex(type, propName);
    if (propIdx < 0) return empty;

    uint8_t valIdx = state.propertyValues[propIdx];
    return type.properties[propIdx].allowedValues[valIdx];
}

// ============================================================================
// Private Helpers
// ============================================================================

uint32_t BlockStateRegistry::ComputePropertyOffset(
    const BlockType& type,
    const std::vector<uint8_t>& propertyValues
) const {
    if (type.properties.empty()) return 0;

    uint32_t offset = 0;
    uint32_t multiplier = 1;

    // Iterate properties in reverse (last property is least significant)
    for (int i = static_cast<int>(type.properties.size()) - 1; i >= 0; --i) {
        offset += propertyValues[i] * multiplier;
        multiplier *= type.properties[i].Cardinality();
    }

    return offset;
}

int BlockStateRegistry::FindPropertyIndex(
    const BlockType& type,
    const std::string& propName
) const {
    for (int i = 0; i < static_cast<int>(type.properties.size()); ++i) {
        if (type.properties[i].name == propName) return i;
    }
    return -1;
}

int BlockStateRegistry::FindValueIndex(
    const BlockProperty& prop,
    const std::string& value
) const {
    for (int i = 0; i < static_cast<int>(prop.allowedValues.size()); ++i) {
        if (prop.allowedValues[i] == value) return i;
    }
    return -1;
}

void BlockStateRegistry::GenerateStatesForType(BlockType& type) {
    // Generate all combinations of property values
    uint32_t totalStates = type.stateCount;

    for (uint32_t i = 0; i < totalStates; ++i) {
        BlockState state;
        state.stateId = type.baseStateId + i;
        state.typeId = type.typeId;

        // Decode the property values from the flat index
        if (!type.properties.empty()) {
            state.propertyValues.resize(type.properties.size());
            uint32_t remaining = i;

            for (int p = static_cast<int>(type.properties.size()) - 1; p >= 0; --p) {
                uint32_t card = type.properties[p].Cardinality();
                state.propertyValues[p] = static_cast<uint8_t>(remaining % card);
                remaining /= card;
            }
        }

        states.push_back(std::move(state));
    }
}

} // namespace blockstate
