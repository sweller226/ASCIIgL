#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace blockstate {

/// A named property with a fixed set of allowed string values.
/// Examples: "facing" → {"north","south","east","west"}, "lit" → {"false","true"}
struct BlockProperty {
    std::string name;
    std::vector<std::string> allowedValues;
    uint8_t defaultIndex = 0;

    uint32_t Cardinality() const { return static_cast<uint32_t>(allowedValues.size()); }
};

} // namespace blockstate
