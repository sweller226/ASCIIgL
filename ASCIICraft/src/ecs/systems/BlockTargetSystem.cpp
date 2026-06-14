#include <ASCIICraft/ecs/systems/BlockTargetSystem.hpp>

#include <array>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/PaletteUtil.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/ecs/components/BlockTarget.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Reach.hpp>
#include <ASCIICraft/util/MeshBuilderUtil.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace ecs::systems {
namespace {

using PosColor = ASCIIgL::VertStructs::PosColor;

constexpr float kEdgeThickness = 0.02f;
constexpr float kOutlineInflate = 1.002f;
constexpr int kOutlineLayer = 5;

void AppendEdgeQuad(
    std::vector<PosColor>& vertices,
    std::vector<int>& indices,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& perpA,
    const glm::vec3& perpB,
    const glm::vec4& color
) {
    const glm::vec3 offset = (perpA + perpB) * kEdgeThickness;
    const std::array<glm::vec3, 4> positions = {{
        a - offset,
        a + perpA * kEdgeThickness - perpB * kEdgeThickness,
        b + perpA * kEdgeThickness - perpB * kEdgeThickness,
        b - offset,
    }};

    std::array<PosColor, 4> corners{};
    for (int i = 0; i < 4; ++i) {
        corners[i].SetXYZ(positions[i]);
        corners[i].SetColor(color);
    }
    util::AppendQuad(vertices, indices, corners);
}

std::shared_ptr<ASCIIgL::Mesh> BuildUnitCubeOutlineMesh(const glm::vec4& color) {
    const glm::vec3 corners[8] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 1.0f},
    };

    const std::array<std::pair<int, int>, 12> edges = {{
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    }};

    std::vector<PosColor> vertices;
    std::vector<int> indices;
    vertices.reserve(edges.size() * 4);
    indices.reserve(edges.size() * 6);

    for (const auto& [i0, i1] : edges) {
        const glm::vec3 a = corners[i0];
        const glm::vec3 b = corners[i1];
        const glm::vec3 delta = b - a;

        glm::vec3 perpA{0.0f};
        glm::vec3 perpB{0.0f};
        if (std::abs(delta.x) > 0.5f) {
            perpA = glm::vec3(0.0f, 1.0f, 0.0f);
            perpB = glm::vec3(0.0f, 0.0f, 1.0f);
        } else if (std::abs(delta.y) > 0.5f) {
            perpA = glm::vec3(1.0f, 0.0f, 0.0f);
            perpB = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            perpA = glm::vec3(1.0f, 0.0f, 0.0f);
            perpB = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        AppendEdgeQuad(vertices, indices, a, b, perpA, perpB, color);
    }

    return std::make_shared<ASCIIgL::Mesh>(
        util::PackVerts(vertices),
        ASCIIgL::VertFormats::PosColor(),
        std::move(indices)
    );
}

} // namespace

BlockTargetSystem::BlockTargetSystem(entt::registry& registry)
    : m_registry(registry)
{}

void BlockTargetSystem::SetGameplayActive(bool gameplayActive) {
    m_gameplayActive = gameplayActive;
}

bool BlockTargetSystem::EnsureRenderResources() {
    if (m_renderResourcesReady) {
        return m_outlineMesh && m_outlineMaterial;
    }

    m_renderResourcesReady = true;

    m_outlineMaterial = ASCIIgL::MaterialLibrary::GetInst().Get("blockTargetOutlineMaterial");
    if (!m_outlineMaterial) {
        ASCIIgL::Logger::Error("BlockTargetSystem: blockTargetOutlineMaterial not found");
        return false;
    }

    const glm::vec4 outlineColor =
        ASCIIgL::PaletteUtil::sRGB255ToLinear1(glm::ivec4(255, 255, 255, 255));
    m_outlineMesh = BuildUnitCubeOutlineMesh(outlineColor);
    if (!m_outlineMesh) {
        ASCIIgL::Logger::Error("BlockTargetSystem: failed to build outline mesh");
        return false;
    }

    return true;
}

void BlockTargetSystem::Update() {
    entt::entity player = components::GetPlayerEntity(m_registry);
    if (player == entt::null) {
        return;
    }

    auto* target = m_registry.try_get<components::BlockTarget>(player);
    if (!target) {
        return;
    }

    target->active = false;
    target->stateId = blockstate::BlockStateRegistry::AIR_STATE_ID;
    target->canPlace = false;

    if (!m_gameplayActive) {
        return;
    }

    auto* cam = m_registry.try_get<components::PlayerCamera>(player);
    auto* reach = m_registry.try_get<components::Reach>(player);
    if (!cam || !reach) {
        return;
    }

    World* world = GetWorldPtr(m_registry);
    ChunkManager* chunkManager = world ? world->GetChunkManager() : nullptr;
    if (!chunkManager) {
        return;
    }

    glm::vec3 origin = cam->camera.pos;
    glm::vec3 lookDir = cam->camera.getCamFront();

    const auto hit = chunkManager->BlockIntersectsView(lookDir, origin, reach->reach);
    if (hit.first != blockstate::BlockStateRegistry::AIR_STATE_ID) {
        target->active = true;
        target->blockPos = hit.second;
        target->stateId = hit.first;
    }

    const auto placement = chunkManager->BlockIntersectsViewForPlacement(lookDir, origin, reach->reach);
    if (placement.first) {
        target->canPlace = true;
        target->placePos = placement.second;
    }
}

void BlockTargetSystem::Render() {
    if (!EnsureRenderResources()) {
        return;
    }

    entt::entity player = components::GetPlayerEntity(m_registry);
    if (player == entt::null) {
        ASCIIgL::Logger::Error("BlockTargetSystem::Render: player entity not found.");
        return;
    }

    const auto* target = m_registry.try_get<components::BlockTarget>(player);
    if (!target) {
        ASCIIgL::Logger::Error("BlockTargetSystem::Render: BlockTarget component missing on player.");
        return;
    }
    if (!target->active) {
        return;
    }

    const auto* cam = m_registry.try_get<components::PlayerCamera>(player);
    if (!cam) {
        ASCIIgL::Logger::Error("BlockTargetSystem::Render: PlayerCamera component missing on player.");
        return;
    }

    const glm::vec3 blockOrigin(
        static_cast<float>(target->blockPos.x),
        static_cast<float>(target->blockPos.y),
        static_cast<float>(target->blockPos.z)
    );

    glm::mat4 model = glm::translate(glm::mat4(1.0f), blockOrigin);
    model = glm::scale(model, glm::vec3(kOutlineInflate));
    const glm::mat4 mvp = cam->camera.proj * cam->camera.view * model;

    ASCIIgL::Renderer::DrawCall dc;
    dc.mesh = m_outlineMesh.get();
    dc.material = m_outlineMaterial.get();
    dc.layer = kOutlineLayer;
    dc.transparent = true;
    dc.depthTest = true;
    dc.backfaceCulling = false;
    dc.sortKey = 0.0f;

    if (const ASCIIgL::UniformDescriptor* desc = m_outlineMaterial->GetUniformDescriptor("mvp")) {
        dc.overrides.push_back({desc, ASCIIgL::UniformValue(mvp)});
    }

    ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
}

} // namespace ecs::systems
