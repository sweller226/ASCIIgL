#include <ASCIICraft/world/block/models/WaterModelBuilder.hpp>
#include <ASCIICraft/world/block/models/ModelBuilderUtil.hpp>

#include <ASCIICraft/textures/BlockTextureCatalog.hpp>
#include <ASCIICraft/world/block/FaceCulling.hpp>

#include <vector>

namespace blockmodels {

namespace {

using modelbuilderutil::V;

static constexpr int kFaceCount = modelbuilderutil::FACE_COUNT;
static constexpr float kShortTopY = 0.8f;

void AppendFace(
    int faceIndex,
    float topY,
    std::vector<V>& verts,
    std::vector<int>& indices
) {
    const int startIndex = static_cast<int>(verts.size());
    const float layer = static_cast<float>(textures::GetLayerForTextureId(
        blocktextures::GetBlockTextureCatalog(),
        "minecraft:blocks/water_still"
    ));
    const auto& faceVerts = modelbuilderutil::GetUnitFaceVerts(faceIndex);
    const auto& faceUVs = modelbuilderutil::GetFaceUVs();
    const auto& faceIndices = modelbuilderutil::GetFaceIndices();

    for (int i = 0; i < modelbuilderutil::VERTS_PER_FACE; ++i) {
        glm::vec3 pos = faceVerts[i];
        if (pos.y > 0.5f) {
            pos.y = topY;
        }

        const glm::vec2 uv = faceUVs[i];
        V v{};
        v.SetXYZ(pos);
        v.SetUV(glm::vec2(uv.x, 1.0f - uv.y));
        v.SetLayer(layer);
        verts.push_back(v);
    }

    for (int i = 0; i < modelbuilderutil::INDICES_PER_FACE; ++i) {
        indices.push_back(startIndex + faceIndices[i]);
    }
}

} // namespace

blockstate::BlockModel BuildWaterModel(const WaterSpec& spec) {
    blockstate::BlockModel out;
    out.isFullBlock = !spec.top;
    out.computeVisibleFaces = faceculling::ComputeVisibleFacesWater;

    blockstate::RenderLayer& layer = out.transparent;
    layer.faces.reserve(kFaceCount);

    const float topY = spec.top ? kShortTopY : 1.0f;
    for (int face = 0; face < kFaceCount; ++face) {
        std::vector<V> faceVerts;
        std::vector<int> faceIndices;
        faceVerts.reserve(4);
        faceIndices.reserve(6);
        AppendFace(face, topY, faceVerts, faceIndices);

        blockstate::FaceRange f;
        f.vertByteOffset = static_cast<int>(layer.vertices.size());
        f.idxOffset = static_cast<int>(layer.indices.size());
        f.cardinalFace = static_cast<uint8_t>(face);

        std::vector<std::byte> packed = modelbuilderutil::PackVerts(faceVerts);
        f.vertByteCount = static_cast<int>(packed.size());
        f.idxCount = static_cast<int>(faceIndices.size());

        layer.vertices.insert(layer.vertices.end(), packed.begin(), packed.end());
        layer.indices.insert(layer.indices.end(), faceIndices.begin(), faceIndices.end());
        layer.faces.push_back(f);
    }

    return out;
}

} // namespace blockmodels
