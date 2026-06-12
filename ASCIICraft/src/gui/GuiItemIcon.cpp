#include <ASCIICraft/gui/GuiItemIcon.hpp>

#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/gui/text/BitmapFont.hpp>
#include <ASCIICraft/gui/text/TextLabelMeshBuilder.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/Material.hpp>

#include <algorithm>
#include <string>
#include <unordered_map>

namespace gui {

namespace {

constexpr float kCountTextScale = 1.0f;
constexpr float kLetterSpacing = -1.1f;
constexpr float kCountTextInsetPx = 1.0f;

std::string CountMeshCacheKey(int count, glm::vec2 boundsPx) {
    return std::to_string(count) + "@" +
           std::to_string(static_cast<int>(boundsPx.x)) + "x" +
           std::to_string(static_cast<int>(boundsPx.y));
}

const gui::text::TextLabelMesh* GetOrBuildCountMesh(const gui::text::BitmapFont& font,
                                                    int count,
                                                    glm::vec2 boundsPx) {
    static std::unordered_map<std::string, gui::text::TextLabelMesh> cache;

    const std::string key = CountMeshCacheKey(count, boundsPx);
    const auto found = cache.find(key);
    if (found != cache.end()) {
        return &found->second;
    }

    gui::text::TextLabelStyle style{};
    style.scale = kCountTextScale;
    style.letterSpacing = kLetterSpacing;
    style.boundsPx = boundsPx;
    style.horizontalAlign = gui::text::TextHAlign::Right;
    style.verticalAlign = gui::text::TextVAlign::Bottom;

    gui::text::TextLabelMesh built = gui::text::TextLabelMeshBuilder::Build(
        font, std::to_string(count), kCountTextScale, style
    );
    if (!built) {
        return nullptr;
    }

    const auto inserted = cache.emplace(key, std::move(built));
    return &inserted.first->second;
}

void DrawItemStackCount(entt::registry& registry,
                        GUIRenderer& renderer,
                        int count,
                        glm::vec2 cellTopLeftPx,
                        glm::vec2 cellSizePx,
                        int layer) {
    if (count <= 1) {
        return;
    }

    const auto* fontPtr = registry.ctx().find<std::shared_ptr<gui::text::BitmapFont>>();
    if (!fontPtr || !*fontPtr) {
        return;
    }

    const glm::vec2 inset{kCountTextInsetPx, kCountTextInsetPx};
    const glm::vec2 bounds = cellSizePx - inset * 2.0f;
    if (bounds.x <= 0.0f || bounds.y <= 0.0f) {
        return;
    }

    const gui::text::TextLabelMesh* cached = GetOrBuildCountMesh(**fontPtr, count, bounds);
    if (!cached || !*cached) {
        return;
    }

    renderer.RenderTextMesh(cellTopLeftPx + inset, layer, cached->mesh);
}

} // namespace

ItemIconRect ComputeItemIconRect(const glm::vec2 cellTopLeftPx, const glm::vec2 cellSizePx) {
    const glm::vec2 iconSize{
        std::min(cellSizePx.x, kItemIconPixels),
        std::min(cellSizePx.y, kItemIconPixels)
    };
    return ItemIconRect{
        cellTopLeftPx + (cellSizePx - iconSize) * 0.5f,
        iconSize
    };
}

bool DrawItemStackIcon(entt::registry& registry,
                       GUIRenderer& renderer,
                       const ecs::components::ItemStack& stack,
                       const glm::vec2 cellTopLeftPx,
                       const glm::vec2 cellSizePx,
                       const int layer) {
    if (stack.isEmpty()) {
        return false;
    }

    auto* itemRegistry = registry.ctx().find<ecs::data::ItemRegistry>();
    if (!itemRegistry) {
        return false;
    }

    const entt::entity proto = itemRegistry->Resolve(stack.itemId);
    if (proto == entt::null) {
        return false;
    }

    auto* visual = registry.try_get<ecs::components::ItemVisual>(proto);
    if (!visual || !visual->mesh) {
        return false;
    }

    const ItemIconRect iconRect = ComputeItemIconRect(cellTopLeftPx, cellSizePx);

    const char* materialName = visual->is2DIcon ? "guiItemMaterial" : "guiBlockMaterial";
    auto material = ASCIIgL::MaterialLibrary::GetInst().Get(materialName);
    if (!material) {
        return false;
    }

    glm::mat4 localModel(1.0f);
    if (!visual->is2DIcon) {
        if (const auto* guiTransform = registry.try_get<ecs::components::ItemGuiMeshTransform>(proto)) {
            localModel = guiTransform->getModel();
        }
    }

    const int countLayer = layer + kItemCountLayerOffset;

    renderer.RenderGUIQuad(
        iconRect.topLeft,
        iconRect.size,
        layer,
        visual->mesh,
        material,
        !visual->is2DIcon,
        localModel
    );
    DrawItemStackCount(registry, renderer, stack.count, cellTopLeftPx, cellSizePx, countLayer);
    return true;
}

} // namespace gui
