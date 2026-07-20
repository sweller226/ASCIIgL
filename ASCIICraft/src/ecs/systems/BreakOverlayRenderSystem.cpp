#include <ASCIICraft/ecs/systems/BreakOverlayRenderSystem.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/ecs/components/MiningProgress.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/textures/BlockTextureCatalog.hpp>
#include <ASCIICraft/textures/TextureCatalog.hpp>
#include <ASCIICraft/util/MeshBuilderUtil.hpp>

namespace ecs::systems {
namespace {

// Slightly larger than the block so the crack sits on top of the block faces
// without z-fighting (same trick as the block target outline).
constexpr float kOverlayInflate = 1.005f;
constexpr int kOverlayLayer = 4;

std::shared_ptr<ASCIIgL::Mesh> BuildUnitCubeOverlayMesh(float textureLayer) {
    using V = ASCIIgL::VertStructs::PosUVLayer;

    std::vector<V> vertices;
    std::vector<int> indices;
    vertices.reserve(24);
    indices.reserve(36);

    const glm::vec2 uv00(0.0f, 1.0f);
    const glm::vec2 uv10(1.0f, 1.0f);
    const glm::vec2 uv11(1.0f, 0.0f);
    const glm::vec2 uv01(0.0f, 0.0f);

    const glm::vec3 p000(0.0f, 0.0f, 0.0f);
    const glm::vec3 p100(1.0f, 0.0f, 0.0f);
    const glm::vec3 p010(0.0f, 1.0f, 0.0f);
    const glm::vec3 p110(1.0f, 1.0f, 0.0f);
    const glm::vec3 p001(0.0f, 0.0f, 1.0f);
    const glm::vec3 p101(1.0f, 0.0f, 1.0f);
    const glm::vec3 p011(0.0f, 1.0f, 1.0f);
    const glm::vec3 p111(1.0f, 1.0f, 1.0f);

    // Drawn with backface culling off, so winding order is not important.
    // North (-Z)
    util::AppendQuadPosUVLayer(vertices, indices, {p000, p100, p110, p010}, {uv00, uv10, uv11, uv01}, textureLayer);
    // South (+Z)
    util::AppendQuadPosUVLayer(vertices, indices, {p101, p001, p011, p111}, {uv00, uv10, uv11, uv01}, textureLayer);
    // West (-X)
    util::AppendQuadPosUVLayer(vertices, indices, {p001, p000, p010, p011}, {uv00, uv10, uv11, uv01}, textureLayer);
    // East (+X)
    util::AppendQuadPosUVLayer(vertices, indices, {p100, p101, p111, p110}, {uv00, uv10, uv11, uv01}, textureLayer);
    // Down (-Y)
    util::AppendQuadPosUVLayer(vertices, indices, {p001, p101, p100, p000}, {uv00, uv10, uv11, uv01}, textureLayer);
    // Up (+Y)
    util::AppendQuadPosUVLayer(vertices, indices, {p010, p110, p111, p011}, {uv00, uv10, uv11, uv01}, textureLayer);

    return std::make_shared<ASCIIgL::Mesh>(
        util::PackVerts(vertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices)
    );
}

} // namespace

BreakOverlayRenderSystem::BreakOverlayRenderSystem(entt::registry& registry)
    : m_registry(registry)
{}

bool BreakOverlayRenderSystem::EnsureRenderResources() {
    if (m_renderResourcesReady) {
        return m_material && m_stageMeshes[0];
    }
    m_renderResourcesReady = true;

    m_material = ASCIIgL::MaterialLibrary::GetInst().Get("breakOverlayMaterial");
    if (!m_material) {
        ASCIIgL::Logger::Error("BreakOverlayRenderSystem: breakOverlayMaterial not found");
        return false;
    }

    const auto& catalog = blocktextures::GetBlockTextureCatalog();
    for (int stage = 0; stage < kStageCount; ++stage) {
        const std::string textureId =
            "minecraft:blocks/destroy_stage_" + std::to_string(stage);
        const int layer = textures::GetLayerForTextureId(catalog, textureId);
        if (layer < 0) {
            ASCIIgL::Logger::Error(
                "BreakOverlayRenderSystem: missing catalog entry for " + textureId);
            m_material = nullptr;
            return false;
        }
        m_stageMeshes[stage] = BuildUnitCubeOverlayMesh(static_cast<float>(layer));
    }

    return true;
}

void BreakOverlayRenderSystem::Render() {
    entt::entity player = components::GetPlayerEntity(m_registry);
    if (player == entt::null) {
        return;
    }

    const auto* mining = m_registry.try_get<components::MiningProgress>(player);
    if (!mining || !mining->active || mining->progress <= 0.0f) {
        return;
    }

    const auto* cam = m_registry.try_get<components::PlayerCamera>(player);
    if (!cam) {
        return;
    }

    if (!EnsureRenderResources()) {
        return;
    }

    const int stage = std::clamp(
        static_cast<int>(mining->progress * static_cast<float>(kStageCount)),
        0, kStageCount - 1);

    const glm::vec3 blockOrigin(
        static_cast<float>(mining->blockPos.x),
        static_cast<float>(mining->blockPos.y),
        static_cast<float>(mining->blockPos.z)
    );
    // Inflate around the block center so all faces get equal clearance.
    const glm::vec3 center = blockOrigin + glm::vec3(0.5f);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, glm::vec3(kOverlayInflate));
    model = glm::translate(model, glm::vec3(-0.5f));
    const glm::mat4 mvp = cam->camera.proj * cam->camera.view * model;

    ASCIIgL::Renderer::DrawCall dc;
    dc.mesh = m_stageMeshes[stage].get();
    dc.material = m_material.get();
    dc.layer = kOverlayLayer;
    dc.transparent = true;
    dc.depthTest = true;
    dc.backfaceCulling = false;
    dc.sortKey = 0.0f;

    if (const ASCIIgL::UniformDescriptor* desc = m_material->GetUniformDescriptor("mvp")) {
        dc.overrides.push_back({desc, ASCIIgL::UniformValue(mvp)});
    }

    ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
}

} // namespace ecs::systems
