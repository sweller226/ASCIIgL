#include <ASCIICraft/ecs/systems/HeldItemRenderSystem.hpp>

#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>

#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace ecs::systems {
namespace {

static const glm::vec3 kRightHandOffset{0.56f, -0.52f, -0.72f};
constexpr int kHeldItemLayer = 1;

glm::mat4 BuildHandLocalModel(const components::ItemHeldMeshTransform* heldPose, bool is2DIcon) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), kRightHandOffset);
    if (heldPose) {
        model = model * heldPose->getModel();
    }
    if (!is2DIcon) {
        model = glm::translate(model, glm::vec3(-0.5f, -0.5f, -0.5f));
    }
    return model;
}

} // namespace

HeldItemRenderSystem::HeldItemRenderSystem(entt::registry& registry)
    : m_registry(registry)
{}

void HeldItemRenderSystem::Render() {
    if (!m_viewModelCamera) {
        return;
    }

    const entt::entity player = components::GetPlayerEntity(m_registry);
    if (player == entt::null) {
        return;
    }

    const auto* inventory = m_registry.try_get<components::Inventory>(player);
    const auto* hotbar = m_registry.try_get<components::HotbarSelection>(player);
    if (!inventory || !hotbar) {
        return;
    }

    if (hotbar->selectedSlot < 0 || hotbar->selectedSlot >= inventory->capacity) {
        return;
    }

    const components::ItemStack& stack = inventory->slots[hotbar->selectedSlot];
    if (stack.isEmpty()) {
        return;
    }

    auto* itemRegistry = m_registry.ctx().find<data::ItemRegistry>();
    if (!itemRegistry) {
        return;
    }

    const entt::entity itemDef = itemRegistry->Resolve(stack.itemId);
    if (itemDef == entt::null) {
        return;
    }

    const auto* visual = m_registry.try_get<components::ItemVisual>(itemDef);
    if (!visual || !visual->droppedMesh) {
        return;
    }

    const char* materialName = visual->is2DIcon ? "heldItemMaterial" : "heldItemBlockMaterial";
    auto material = ASCIIgL::MaterialLibrary::GetInst().Get(materialName);
    if (!material) {
        return;
    }

    const auto* heldPose = m_registry.try_get<components::ItemHeldMeshTransform>(itemDef);
    const glm::mat4 model = BuildHandLocalModel(heldPose, visual->is2DIcon);
    const glm::mat4 mvp = m_viewModelCamera->proj * m_viewModelCamera->view * model;

    ASCIIgL::Renderer::DrawCall dc;
    dc.mesh = visual->droppedMesh.get();
    dc.material = material.get();
    dc.layer = kHeldItemLayer;
    dc.sortKey = 0.0f;
    dc.backfaceCulling = false;
    dc.transparent = true;
    // Viewmodel camera; depth buffer was filled using the world camera — disable depth test.
    dc.depthTest = false;

    if (const ASCIIgL::UniformDescriptor* mvpDesc = material->GetUniformDescriptor("mvp")) {
        dc.overrides.push_back({mvpDesc, ASCIIgL::UniformValue(mvp)});
    }

    ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
}

void HeldItemRenderSystem::SetViewModelCamera(ASCIIgL::Camera3D* camera3D) {
    m_viewModelCamera = camera3D;
}

} // namespace ecs::systems
