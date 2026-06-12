#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

#include <ASCIICraft/util/QuadMeshBuilder.hpp>
#include <ASCIICraft/world/block/models/BlockModelMeshBuilder.hpp>
#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>

#include <ASCIICraft/ecs/components/ItemDefinitionTag.hpp>
#include <ASCIICraft/ecs/components/ItemId.hpp>
#include <ASCIICraft/ecs/components/ItemProperties.hpp>
#include <ASCIICraft/ecs/components/Stackable.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>

namespace ecs::data {

// ============================================================================
// Item Definition Builders
// ============================================================================

entt::entity ItemRegistry::RegisterBlockItem(
    entt::registry& reg,
    const std::string& name, const std::string& display,
    int maxStack,
    std::optional<components::ItemGuiMeshTransform> guiTransform
) {
    auto e = reg.create();
    int id = RegisterNext(name, e);
    reg.emplace<components::ItemDefinitionTag>(e);
    reg.emplace<components::ItemId>(e, id, name, display);
    reg.emplace<components::Stackable>(e, maxStack);

    int blockTypeId = id;
    if (auto* bsr = reg.ctx().find<blockstate::BlockStateRegistry>()) {
        blockTypeId = static_cast<int>(bsr->GetTypeId(name));
    }
    reg.emplace<components::PlaceableProperty>(e, blockTypeId);

    auto& visual = reg.emplace<components::ItemVisual>(e);
    visual.is2DIcon = false;
    visual.mesh = buildBlockItemMesh(reg, e);
    reg.emplace<components::ItemGuiMeshTransform>(
        e,
        guiTransform.value_or(components::ItemGuiMeshTransform::DefaultBlockThirdPerson()));
    return e;
}

entt::entity ItemRegistry::RegisterResourceItem(
    entt::registry& reg,
    const std::string& name, const std::string& display,
    float iconLayer, int maxStack
) {
    auto e = reg.create();
    int id = RegisterNext(name, e);
    components::ItemVisual visual;
    visual.is2DIcon = true;
    visual.mesh = buildIconMesh(iconLayer);
    reg.emplace<components::ItemDefinitionTag>(e);
    reg.emplace<components::ItemId>(e, id, name, display);
    reg.emplace<components::Stackable>(e, maxStack);
    reg.emplace<components::ItemVisual>(e, std::move(visual));
    return e;
}

entt::entity ItemRegistry::RegisterToolItem(
    entt::registry& reg,
    const std::string& name, const std::string& display,
    float iconLayer,
    components::ToolProperty tool, components::WeaponProperty weapon
) {
    auto e = reg.create();
    int id = RegisterNext(name, e);
    components::ItemVisual visual;
    visual.is2DIcon = true;
    visual.mesh = buildIconMesh(iconLayer);
    reg.emplace<components::ItemDefinitionTag>(e);
    reg.emplace<components::ItemId>(e, id, name, display);
    reg.emplace<components::ItemVisual>(e, std::move(visual));
    reg.emplace<components::ToolProperty>(e, tool);
    reg.emplace<components::WeaponProperty>(e, weapon);
    return e;
}

// ============================================================================
// Registry Operations
// ============================================================================

bool ItemRegistry::Register(int id, const std::string& name, entt::entity entity) {
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

int ItemRegistry::RegisterNext(const std::string& name, entt::entity entity) {
    int id = nextId;
    if (!Register(id, name, entity)) {
        return -1;
    }
    return id;
}

entt::entity ItemRegistry::Resolve(int id) const {
    auto it = byId.find(id);
    if (it != byId.end()) {
        return it->second;
    }
    return entt::null;
}

entt::entity ItemRegistry::Resolve(const std::string& name) const {
    auto it = byName.find(name);
    if (it != byName.end()) {
        return it->second;
    }
    return entt::null;
}

bool ItemRegistry::Exists(int id) const {
    return byId.find(id) != byId.end();
}

std::string ItemRegistry::GetNameFromId(int id, entt::registry& reg) const {
    const entt::entity e = Resolve(id);
    if (e == entt::null) {
        return {};
    }
    return reg.get<components::ItemId>(e).registryName;
}

int ItemRegistry::GetIdFromName(const std::string& name, entt::registry& reg) const {
    const entt::entity e = Resolve(name);
    if (e == entt::null) {
        return -1;
    }
    return reg.get<components::ItemId>(e).numericId;
}

void ItemRegistry::Clear() {
    byId.clear();
    byName.clear();
}

std::shared_ptr<ASCIIgL::Mesh> ItemRegistry::buildIconMesh(float iconLayer) const {
    auto itemArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("itemTextureArray");
    if (!itemArray) {
        return nullptr;
    }
    return util::QuadMeshBuilder::BuildPosUVLayerQuad(itemArray, iconLayer);
}

std::shared_ptr<ASCIIgL::Mesh> ItemRegistry::buildBlockItemMesh(
    entt::registry& reg,
    entt::entity definitionEntity
) const {
    const auto* placeable = reg.try_get<components::PlaceableProperty>(definitionEntity);
    if (!placeable || placeable->typeId < 0) {
        return nullptr;
    }

    auto* bsr = reg.ctx().find<blockstate::BlockStateRegistry>();
    auto* modelLibrary = reg.ctx().find<blockmodels::BlockModelLibrary>();
    if (!bsr || !modelLibrary) {
        return nullptr;
    }

    auto terrainArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
    if (!terrainArray) {
        return nullptr;
    }

    const uint32_t stateId = bsr->GetDefaultState(static_cast<uint16_t>(placeable->typeId));
    const blockstate::BlockModel* model = modelLibrary->GetModel(stateId);
    if (!model) {
        return nullptr;
    }

    return blockmodels::BuildMesh(*model, terrainArray.get());
}

} // namespace ecs::data
