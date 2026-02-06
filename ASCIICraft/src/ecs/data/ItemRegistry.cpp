#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ecs::data {

// ============================================================================
// Item Registry
// ============================================================================

ItemRegistry& ItemRegistry::Instance() {
    static ItemRegistry instance;
    return instance;
}

bool ItemRegistry::RegisterItem(const ItemDefinition& def) {
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

std::shared_ptr<ASCIIgL::Mesh> ItemRegistry::GetQuadItemMesh(int layer)
{
    using V = ASCIIgL::VertStructs::PosUVLayer;

    std::vector<V> vertices;
    std::vector<int> indices;

    // A simple 2D quad in the XY plane (Z = 0)
    // CCW winding
    vertices.push_back({ -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, (float)layer }); // bottom-left
    vertices.push_back({ -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, (float)layer }); // top-left
    vertices.push_back({  1.0f,  1.0f, 0.0f, 1.0f, 1.0f, (float)layer }); // top-right
    vertices.push_back({  1.0f, -1.0f, 0.0f, 1.0f, 0.0f, (float)layer }); // bottom-right

    // Two triangles
    indices = { 0, 1, 2, 0, 2, 3 };

    // Convert vertices to byte buffer
    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices),
        Block::GetTextureArray()
    );
}

std::shared_ptr<ASCIIgL::Mesh> ItemRegistry::GetQuadItemMesh(int x, int y, int ATLAS_SIZE) {
    int layer = ASCIIgL::TextureArray::GetLayerFromAtlasXY(x, y, ATLAS_SIZE);
    return ItemRegistry::GetQuadItemMesh(layer);
}

std::string ItemRegistry::MakeItemName(const std::string& name) {
    return "minecraft:" + name;
}

ItemDefinition ItemRegistry::MakeItemDef(
    int id,
    const std::string& name,
    const std::string& displayName,
    int maxStackSize,
    bool isStackable,
    std::shared_ptr<ASCIIgL::Mesh> mesh,
    bool is2DIcon
) {
    ItemDefinition def;
    def.id = id;
    def.name = MakeItemName(name);
    def.displayName = displayName;
    def.maxStackSize = maxStackSize;
    def.isStackable = isStackable;
    def.mesh = mesh;
    def.is2DIcon = is2DIcon;
    return def;
}

} // namespace ecs::data
