#include <ASCIICraft/ecs/systems/BreakOverlayRenderSystem.hpp>

#include <algorithm>
#include <cstring>
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
#include <ASCIICraft/world/block/models/BlockModel.hpp>
#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

#include <glm/geometric.hpp>

namespace ecs::systems {
namespace {

// Slightly larger than the block so the crack sits on top of the block faces
// without z-fighting (same trick as the block target outline).
constexpr float kOverlayInflate = 1.005f;
/// Push each face along its outward normal. Needed because center-scale inflate leaves
/// faces that pass through the block center (e.g. bottom-slab top at y=0.5) unmoved.
constexpr float kOverlayFaceBias = 0.0025f;
constexpr int kOverlayLayer = 4;

const glm::vec2 kFaceUv00(0.0f, 1.0f);
const glm::vec2 kFaceUv10(1.0f, 1.0f);
const glm::vec2 kFaceUv11(1.0f, 0.0f);
const glm::vec2 kFaceUv01(0.0f, 0.0f);

bool ModelHasOverlayFaces(const blockstate::BlockModel& model) {
    return !model.opaque.faces.empty() || !model.transparent.faces.empty();
}

/// Map block-local position to destroy-stage UVs at 1:1 world scale (no per-face stretch).
/// One block unit = one full texture; partial faces show a cropped region.
glm::vec2 OverlayUvFromPosition(const glm::vec3& p, const glm::vec3& faceNormal) {
    const glm::vec3 n = glm::abs(faceNormal);
    if (n.y >= n.x && n.y >= n.z) {
        // Top / bottom: project onto XZ
        return glm::vec2(p.x, 1.0f - p.z);
    }
    if (n.x >= n.z) {
        // East / west: project onto ZY
        return glm::vec2(p.z, 1.0f - p.y);
    }
    // North / south: project onto XY
    return glm::vec2(p.x, 1.0f - p.y);
}

glm::vec3 FaceNormalFromVerts(const std::vector<ASCIIgL::VertStructs::PosUVLayer>& verts) {
    if (verts.size() < 3) {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }
    const glm::vec3 p0 = verts[0].GetXYZ();
    const glm::vec3 p1 = verts[1].GetXYZ();
    const glm::vec3 p2 = verts[2].GetXYZ();
    const glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
    const float len = glm::length(n);
    if (len < 1e-8f) {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }
    return n / len;
}

glm::vec3 FaceNormalFromCardinal(uint8_t cardinalFace) {
    if (cardinalFace > 5) {
        return glm::vec3(0.0f);
    }
    return FaceDirOutwardNormal(FaceDirFromIndex(static_cast<int>(cardinalFace)));
}

void AppendOverlayFacesFromLayer(
    std::vector<ASCIIgL::VertStructs::PosUVLayer>& vertices,
    std::vector<int>& indices,
    const blockstate::RenderLayer& layer,
    float textureLayer
) {
    using V = ASCIIgL::VertStructs::PosUVLayer;

    for (const blockstate::FaceRange& face : layer.faces) {
        if (face.vertByteCount <= 0 || (face.vertByteCount % static_cast<int>(sizeof(V)) != 0)) {
            continue;
        }
        const int vertCount = face.vertByteCount / static_cast<int>(sizeof(V));
        if (vertCount <= 0) {
            continue;
        }

        std::vector<V> srcVerts(static_cast<size_t>(vertCount));
        std::memcpy(
            srcVerts.data(),
            layer.vertices.data() + static_cast<size_t>(face.vertByteOffset),
            static_cast<size_t>(face.vertByteCount)
        );

        glm::vec3 faceNormal = FaceNormalFromCardinal(face.cardinalFace);
        if (glm::length(faceNormal) < 0.5f) {
            faceNormal = FaceNormalFromVerts(srcVerts);
        }

        // Prefer the outward direction (away from block center) so bias never pushes into the mesh.
        glm::vec3 faceCentroid(0.0f);
        for (const V& v : srcVerts) {
            faceCentroid += v.GetXYZ();
        }
        faceCentroid /= static_cast<float>(vertCount);
        if (glm::dot(faceNormal, faceCentroid - glm::vec3(0.5f)) < 0.0f) {
            faceNormal = -faceNormal;
        }

        const glm::vec3 bias = faceNormal * kOverlayFaceBias;
        const int baseVertex = static_cast<int>(vertices.size());
        for (int i = 0; i < vertCount; ++i) {
            const glm::vec3 pos = srcVerts[static_cast<size_t>(i)].GetXYZ();
            V out{};
            out.SetXYZ(pos + bias);
            out.SetUV(OverlayUvFromPosition(pos, faceNormal));
            out.SetLayer(textureLayer);
            vertices.push_back(out);
        }

        for (int j = 0; j < face.idxCount; ++j) {
            indices.push_back(baseVertex + layer.indices[face.idxOffset + j]);
        }
    }
}

std::shared_ptr<ASCIIgL::Mesh> BuildBreakOverlayMeshFromModel(
    const blockstate::BlockModel& model,
    float textureLayer
) {
    using V = ASCIIgL::VertStructs::PosUVLayer;

    std::vector<V> vertices;
    std::vector<int> indices;
    const size_t faceEstimate = model.opaque.faces.size() + model.transparent.faces.size();
    vertices.reserve(faceEstimate * 4);
    indices.reserve(faceEstimate * 6);

    AppendOverlayFacesFromLayer(vertices, indices, model.opaque, textureLayer);
    AppendOverlayFacesFromLayer(vertices, indices, model.transparent, textureLayer);

    if (vertices.empty() || indices.empty()) {
        return nullptr;
    }

    return std::make_shared<ASCIIgL::Mesh>(
        util::PackVerts(vertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices)
    );
}

std::shared_ptr<ASCIIgL::Mesh> BuildUnitCubeOverlayMesh(float textureLayer) {
    using V = ASCIIgL::VertStructs::PosUVLayer;

    std::vector<V> vertices;
    std::vector<int> indices;
    vertices.reserve(24);
    indices.reserve(36);

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
    util::AppendQuadPosUVLayer(vertices, indices, {p000, p100, p110, p010}, {kFaceUv00, kFaceUv10, kFaceUv11, kFaceUv01}, textureLayer);
    // South (+Z)
    util::AppendQuadPosUVLayer(vertices, indices, {p101, p001, p011, p111}, {kFaceUv00, kFaceUv10, kFaceUv11, kFaceUv01}, textureLayer);
    // West (-X)
    util::AppendQuadPosUVLayer(vertices, indices, {p001, p000, p010, p011}, {kFaceUv00, kFaceUv10, kFaceUv11, kFaceUv01}, textureLayer);
    // East (+X)
    util::AppendQuadPosUVLayer(vertices, indices, {p100, p101, p111, p110}, {kFaceUv00, kFaceUv10, kFaceUv11, kFaceUv01}, textureLayer);
    // Down (-Y)
    util::AppendQuadPosUVLayer(vertices, indices, {p001, p101, p100, p000}, {kFaceUv00, kFaceUv10, kFaceUv11, kFaceUv01}, textureLayer);
    // Up (+Y)
    util::AppendQuadPosUVLayer(vertices, indices, {p010, p110, p111, p011}, {kFaceUv00, kFaceUv10, kFaceUv11, kFaceUv01}, textureLayer);

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

void BreakOverlayRenderSystem::ClearShapeCache() {
    m_shapeMesh.reset();
    m_cachedStateId = 0;
    m_cachedStage = -1;
}

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
        m_stageLayers[stage] = static_cast<float>(layer);
        m_stageMeshes[stage] = BuildUnitCubeOverlayMesh(m_stageLayers[stage]);
    }

    return true;
}

ASCIIgL::Mesh* BreakOverlayRenderSystem::ResolveOverlayMesh(uint32_t stateId, int stage) {
    const auto* modelLibrary = m_registry.ctx().find<blockmodels::BlockModelLibrary>();
    const blockstate::BlockModel* model =
        modelLibrary ? modelLibrary->GetModel(stateId) : nullptr;

    if (!model || !ModelHasOverlayFaces(*model)) {
        ClearShapeCache();
        return m_stageMeshes[stage].get();
    }

    if (m_shapeMesh && m_cachedStateId == stateId && m_cachedStage == stage) {
        return m_shapeMesh.get();
    }

    m_shapeMesh = BuildBreakOverlayMeshFromModel(*model, m_stageLayers[stage]);
    m_cachedStateId = stateId;
    m_cachedStage = stage;

    if (!m_shapeMesh) {
        ClearShapeCache();
        return m_stageMeshes[stage].get();
    }
    return m_shapeMesh.get();
}

void BreakOverlayRenderSystem::Render() {
    entt::entity player = components::GetPlayerEntity(m_registry);
    if (player == entt::null) {
        return;
    }

    const auto* mining = m_registry.try_get<components::MiningProgress>(player);
    if (!mining || !mining->active || mining->progress <= 0.0f) {
        ClearShapeCache();
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

    ASCIIgL::Mesh* mesh = ResolveOverlayMesh(mining->stateId, stage);
    if (!mesh) {
        return;
    }

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
    dc.mesh = mesh;
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
