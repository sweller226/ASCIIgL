#include <ASCIICraft/gui/GuiItemIcon.hpp>

#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

#include <ASCIIgL/renderer/Material.hpp>

#include <algorithm>

namespace gui {

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

    const glm::vec2 iconSize{
        std::min(cellSizePx.x, kItemIconPixels),
        std::min(cellSizePx.y, kItemIconPixels)
    };
    const glm::vec2 iconTopLeft = cellTopLeftPx + (cellSizePx - iconSize) * 0.5f;

    auto guiItemMat = ASCIIgL::MaterialLibrary::GetInst().Get("guiItemMaterial");
    renderer.RenderGUIQuad(
        iconTopLeft,
        iconSize,
        layer,
        visual->mesh,
        guiItemMat,
        !visual->is2DIcon
    );
    return true;
}

} // namespace gui
