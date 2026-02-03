#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

namespace ecs::data {

ItemRegistry& ItemRegistry::Instance() {
    static ItemRegistry instance;
    return instance;
}

bool ItemRegistry::registerItem(const ItemDefinition& def) {
    // Validate
    if (def.id < 0) return false;
    if (def.name.empty()) return false;

    // Check for duplicates
    if (itemsById.find(def.id) != itemsById.end()) return false;
    if (nameToId.find(def.name) != nameToId.end()) return false;

    // Register
    itemsById[def.id] = def;
    nameToId[def.name] = def.id;
    return true;
}

const ItemDefinition* ItemRegistry::getById(int id) const {
    auto it = itemsById.find(id);
    if (it != itemsById.end()) {
        return &it->second;
    }
    return nullptr;
}

const ItemDefinition* ItemRegistry::getByName(const std::string& name) const {
    auto it = nameToId.find(name);
    if (it != nameToId.end()) {
        return getById(it->second);
    }
    return nullptr;
}

bool ItemRegistry::exists(int id) const {
    return itemsById.find(id) != itemsById.end();
}

void ItemRegistry::clear() {
    itemsById.clear();
    nameToId.clear();
}

} // namespace ecs::data
