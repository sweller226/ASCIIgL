#include <ASCIICraft/ecs/systems/HeldItemRenderSystem.hpp>

#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>

#include <ASCIICraft/ecs/components/HandSwing.hpp>
#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/ViewBobbing.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>
#include <ASCIICraft/ecs/ViewBobbingMath.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace ecs::systems {
namespace {

static const glm::vec3 kRightHandOffset{0.56f, -0.52f, -0.72f};
constexpr int kHeldItemLayer = 1;
constexpr float kPi = 3.14159265358979f;

/// Minecraft's first-person swing (ItemRenderer.transformFirstPerson):
/// a translation arc plus rotations driven by sin curves of the progress.
glm::mat4 BuildSwingMatrix(float progress) {
    if (progress <= 0.0f) {
        return glm::mat4(1.0f);
    }

    const float sqrtP = std::sqrt(progress);
    const glm::vec3 swingTranslate(
        -0.4f * std::sin(sqrtP * kPi),
         0.2f * std::sin(sqrtP * kPi * 2.0f),
        -0.2f * std::sin(progress * kPi)
    );

    const float f  = std::sin(progress * progress * kPi);
    const float f1 = std::sin(sqrtP * kPi);

    glm::mat4 m = glm::translate(glm::mat4(1.0f), swingTranslate);
    m = glm::rotate(m, glm::radians(45.0f),        glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(f * -20.0f),   glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(f1 * -20.0f),  glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::rotate(m, glm::radians(f1 * -80.0f),  glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(-45.0f),       glm::vec3(0.0f, 1.0f, 0.0f));
    return m;
}

glm::mat4 BuildHandLocalModel(
    const components::ItemHeldMeshTransform* heldPose,
    bool is2DIcon,
    float swingProgress
) {
    glm::mat4 model = BuildSwingMatrix(swingProgress);
    model = glm::translate(model, kRightHandOffset);
    if (heldPose) {
        model = model * heldPose->getModel();
    }
    if (!is2DIcon) {
        model = glm::translate(model, glm::vec3(-0.5f, -0.5f, -0.5f));
    }
    return model;
}

glm::mat4 BuildHeldItemBobMatrix(const components::ViewBobbing* bob,
                                 const components::Transform* transform) {
    if (!bob || !transform || !bob->enabled || bob->cameraYaw <= 1e-4f) {
        return glm::mat4(1.0f);
    }

    const float renderAlpha = viewbobbing::ComputeRenderAlpha(*transform);
    const float walkDelta = bob->distanceWalkedModified - bob->prevDistanceWalkedModified;
    const float walkPhase = -(bob->distanceWalkedModified + walkDelta * renderAlpha);

    const float interpCameraYaw = bob->prevCameraYaw
        + (bob->cameraYaw - bob->prevCameraYaw) * renderAlpha;
    const float interpCameraPitch = bob->prevCameraPitch
        + (bob->cameraPitch - bob->prevCameraPitch) * renderAlpha;

    return viewbobbing::BuildMinecraftBobMatrix(
        walkPhase,
        interpCameraYaw,
        interpCameraPitch,
        bob->intensity
    );
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
    const auto* bob = m_registry.try_get<components::ViewBobbing>(player);
    const auto* transform = m_registry.try_get<components::Transform>(player);
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
    const auto* swing = m_registry.try_get<components::HandSwing>(player);
    const float swingProgress = (swing && swing->swinging) ? swing->progress : 0.0f;
    const glm::mat4 model = BuildHandLocalModel(heldPose, visual->is2DIcon, swingProgress);
    const glm::mat4 bobMatrix = BuildHeldItemBobMatrix(bob, transform);
    const glm::mat4 mvp = m_viewModelCamera->proj * bobMatrix * m_viewModelCamera->view * model;

    ASCIIgL::Renderer::DrawCall dc;
    dc.mesh = visual->droppedMesh.get();
    dc.material = material.get();
    dc.layer = kHeldItemLayer;
    dc.sortKey = 0.0f;
    dc.backfaceCulling = true;
    dc.transparent = true;
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
