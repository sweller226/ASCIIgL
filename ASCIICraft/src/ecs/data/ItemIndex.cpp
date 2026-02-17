#include <ASCIICraft/ecs/data/ItemIndex.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <ASCIICraft/world/blockstate/BlockState.hpp>
#include <ASCIICraft/world/blockstate/BlockFace.hpp>

#include <glm/glm.hpp>

#include <ASCIICraft/ecs/components/ItemDefinitionTag.hpp>
#include <ASCIICraft/ecs/components/ItemId.hpp>
#include <ASCIICraft/ecs/components/Stackable.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>

namespace ecs::data {

// ============================================================================
// Item Definition Builders
// ============================================================================

entt::entity ItemIndex::RegisterBlockItem(
    entt::registry& reg,
    const std::string& name, const std::string& display,
    std::shared_ptr<ASCIIgL::Mesh> mesh, int maxStack
) {
    auto e = reg.create();
    int id = RegisterNext(name, e);
    reg.emplace<components::ItemDefinitionTag>(e);
    reg.emplace<components::ItemId>(e, id, name, display);
    reg.emplace<components::Stackable>(e, maxStack);
    reg.emplace<components::ItemVisual>(e, std::move(mesh), false);
    reg.emplace<components::PlaceableProperty>(e, id);  // blockId == itemId
    return e;
}

entt::entity ItemIndex::RegisterResourceItem(
    entt::registry& reg,
    int id, const std::string& name, const std::string& display,
    std::shared_ptr<ASCIIgL::Mesh> mesh, int maxStack
) {
    auto e = reg.create();
    reg.emplace<components::ItemDefinitionTag>(e);
    reg.emplace<components::ItemId>(e, id, name, display);
    reg.emplace<components::Stackable>(e, maxStack);
    reg.emplace<components::ItemVisual>(e, std::move(mesh), true);
    Register(id, name, e);
    return e;
}

entt::entity ItemIndex::RegisterToolItem(
    entt::registry& reg,
    int id, const std::string& name, const std::string& display,
    std::shared_ptr<ASCIIgL::Mesh> mesh,
    components::ToolProperty tool, components::WeaponProperty weapon
) {
    auto e = reg.create();
    reg.emplace<components::ItemDefinitionTag>(e);
    reg.emplace<components::ItemId>(e, id, name, display);
    reg.emplace<components::ItemVisual>(e, std::move(mesh), true);
    reg.emplace<components::ToolProperty>(e, tool);
    reg.emplace<components::WeaponProperty>(e, weapon);
    Register(id, name, e);
    return e;
}

// ============================================================================
// Index Operations
// ============================================================================

bool ItemIndex::Register(int id, const std::string& name, entt::entity entity) {
    if (id < 0) return false;
    if (name.empty()) return false;

    // Check for duplicates
    if (byId.find(id) != byId.end()) return false;
    if (byName.find(name) != byName.end()) return false;

    byId[id] = entity;
    byName[name] = entity;
    // Advance counter past this ID if needed
    if (id >= nextId) {
        nextId = id + 1;
    }
    return true;
}

int ItemIndex::RegisterNext(const std::string& name, entt::entity entity) {
    int id = nextId;
    if (!Register(id, name, entity)) {
        return -1;
    }
    return id;
}

entt::entity ItemIndex::Resolve(int id) const {
    auto it = byId.find(id);
    if (it != byId.end()) {
        return it->second;
    }
    return entt::null;
}

entt::entity ItemIndex::Resolve(const std::string& name) const {
    auto it = byName.find(name);
    if (it != byName.end()) {
        return it->second;
    }
    return entt::null;
}

bool ItemIndex::Exists(int id) const {
    return byId.find(id) != byId.end();
}

void ItemIndex::Clear() {
    byId.clear();
    byName.clear();
}

std::shared_ptr<ASCIIgL::Mesh> ItemIndex::GetQuadItemMesh(int layer)
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
        ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray").get()
    );
}

std::shared_ptr<ASCIIgL::Mesh> ItemIndex::GetBlockMeshFromState(const blockstate::BlockState& state) {
    using V = ASCIIgL::VertStructs::PosUVLayer;

    std::vector<V> vertices;
    std::vector<int> indices;

    auto pushFace = [&](BlockFace face,
                        const glm::vec3& v0,
                        const glm::vec3& v1,
                        const glm::vec3& v2,
                        const glm::vec3& v3)
    {
        int layer = state.faceTextureLayers[static_cast<int>(face)];

        int startIndex = static_cast<int>(vertices.size());

        vertices.push_back({ v0.x, v0.y, v0.z, 0.0f, 0.0f, (float)layer });
        vertices.push_back({ v1.x, v1.y, v1.z, 0.0f, 1.0f, (float)layer });
        vertices.push_back({ v2.x, v2.y, v2.z, 1.0f, 1.0f, (float)layer });
        vertices.push_back({ v3.x, v3.y, v3.z, 1.0f, 0.0f, (float)layer });

        indices.push_back(startIndex + 0);
        indices.push_back(startIndex + 1);
        indices.push_back(startIndex + 2);

        indices.push_back(startIndex + 0);
        indices.push_back(startIndex + 2);
        indices.push_back(startIndex + 3);
    };

    // Cube corners
    glm::vec3 p000(-1, -1, -1);
    glm::vec3 p001(-1, -1,  1);
    glm::vec3 p010(-1,  1, -1);
    glm::vec3 p011(-1,  1,  1);
    glm::vec3 p100( 1, -1, -1);
    glm::vec3 p101( 1, -1,  1);
    glm::vec3 p110( 1,  1, -1);
    glm::vec3 p111( 1,  1,  1);

    // Top (+Y)
    pushFace(BlockFace::Top,    p011, p111, p110, p010);
    // Bottom (-Y)
    pushFace(BlockFace::Bottom, p001, p000, p100, p101);
    // North (+Z)
    pushFace(BlockFace::North,  p001, p011, p111, p101);
    // South (-Z)
    pushFace(BlockFace::South,  p100, p110, p010, p000);
    // East (+X)
    pushFace(BlockFace::East,   p101, p111, p110, p100);
    // West (-X)
    pushFace(BlockFace::West,   p000, p010, p011, p001);

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices),
        ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray").get()
    );
}

std::shared_ptr<ASCIIgL::Mesh> ItemIndex::GetQuadItemMesh(int x, int y, int ATLAS_SIZE) {
    int layer = ASCIIgL::TextureArray::GetLayerFromAtlasXY(x, y, ATLAS_SIZE);
    return ItemIndex::GetQuadItemMesh(layer);
}

} // namespace ecs::data
