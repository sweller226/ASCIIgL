#include <ASCIICraft/world/block/models/CubeModelBuilder.hpp>
#include <ASCIICraft/world/block/models/ModelBuilderUtil.hpp>
#include <ASCIICraft/util/MeshBuilderUtil.hpp>

#include <vector>

#include <glm/vec2.hpp>

#include <ASCIICraft/world/block/FaceCulling.hpp>

namespace blockmodels {

namespace {

using modelbuilderutil::V;

static constexpr int kFaceCount = modelbuilderutil::FACE_COUNT;

inline glm::vec2 RotateTopBottomUVLocal(glm::vec2 uv, FaceDir facing) {
    float u = uv.x, v = uv.y;
    switch (facing) {
        case FaceDir::North: return uv;                           // 0Â°
        case FaceDir::South: return glm::vec2(1.0f - u, 1.0f - v); // 180Â°
        case FaceDir::East:  return glm::vec2(1.0f - v, u);        // 90Â° CW
        case FaceDir::West:  return glm::vec2(v, 1.0f - u);        // 270Â° CW
        default:             return uv;
    }
}

inline int SourceFaceForWorldFace(int worldFace, FaceDir facing) {
    // CubeSpec face layers are authored in local-space as if facing North.
    // This maps world face -> local source face for yaw-rotated placements.
    if (worldFace == static_cast<int>(FaceDir::Top) || worldFace == static_cast<int>(FaceDir::Bottom)) {
        return worldFace;
    }
    switch (facing) {
        case FaceDir::North:
            return worldFace;
        case FaceDir::East:
            switch (worldFace) {
                case static_cast<int>(FaceDir::North): return static_cast<int>(FaceDir::West);
                case static_cast<int>(FaceDir::East):  return static_cast<int>(FaceDir::North);
                case static_cast<int>(FaceDir::South): return static_cast<int>(FaceDir::East);
                case static_cast<int>(FaceDir::West):  return static_cast<int>(FaceDir::South);
                default: return worldFace;
            }
        case FaceDir::South:
            switch (worldFace) {
                case static_cast<int>(FaceDir::North): return static_cast<int>(FaceDir::South);
                case static_cast<int>(FaceDir::East):  return static_cast<int>(FaceDir::West);
                case static_cast<int>(FaceDir::South): return static_cast<int>(FaceDir::North);
                case static_cast<int>(FaceDir::West):  return static_cast<int>(FaceDir::East);
                default: return worldFace;
            }
        case FaceDir::West:
            switch (worldFace) {
                case static_cast<int>(FaceDir::North): return static_cast<int>(FaceDir::East);
                case static_cast<int>(FaceDir::East):  return static_cast<int>(FaceDir::South);
                case static_cast<int>(FaceDir::South): return static_cast<int>(FaceDir::West);
                case static_cast<int>(FaceDir::West):  return static_cast<int>(FaceDir::North);
                default: return worldFace;
            }
        default:
            return worldFace;
    }
}

inline void AppendFace(
    const CubeSpec& spec,
    int worldFaceIndex,
    std::vector<V>& verts,
    std::vector<int>& indices
) {
    const int sourceFaceIndex = SourceFaceForWorldFace(worldFaceIndex, spec.facing);
    const int layer = spec.faceLayers[sourceFaceIndex];

    const auto& unitFaceVerts = modelbuilderutil::GetUnitFaceVerts(worldFaceIndex);
    const auto& faceUVs = modelbuilderutil::GetFaceUVs();

    std::array<V, modelbuilderutil::VERTS_PER_FACE> corners{};
    for (int vi = 0; vi < modelbuilderutil::VERTS_PER_FACE; ++vi) {
        const glm::vec3 pos = unitFaceVerts[vi];

        glm::vec2 uv = faceUVs[vi];
        if (worldFaceIndex == static_cast<int>(FaceDir::Top) || worldFaceIndex == static_cast<int>(FaceDir::Bottom)) {
            uv = RotateTopBottomUVLocal(uv, spec.facing);
        }

        // Match ChunkMeshGen: flip V.
        const float u = uv.x;
        const float v = 1.0f - uv.y;

        corners[vi].SetXYZ(pos);
        corners[vi].SetUV(glm::vec2(u, v));
        corners[vi].SetLayer(static_cast<float>(layer));
    }

    util::AppendQuad(verts, indices, corners);
}

} // namespace

blockstate::BlockModel BuildCubeModel(const CubeSpec& spec) {
    blockstate::BlockModel out;
    out.isFullBlock = true;
    out.computeVisibleFaces = faceculling::ComputeVisibleFacesFullBlock;

    blockstate::RenderLayer& layer = spec.transparent ? out.transparent : out.opaque;
    layer.faces.reserve(kFaceCount);

    for (int face = 0; face < kFaceCount; ++face) {
        std::vector<V> verts;
        std::vector<int> indices;
        verts.reserve(4);
        indices.reserve(6);

        AppendFace(spec, face, verts, indices);

        blockstate::FaceRange f;
        f.vertByteOffset = static_cast<int>(layer.vertices.size());
        f.idxOffset      = static_cast<int>(layer.indices.size());
        f.cardinalFace   = static_cast<uint8_t>(face);

        std::vector<std::byte> packed = util::PackVerts(verts);
        f.vertByteCount = static_cast<int>(packed.size());
        f.idxCount      = static_cast<int>(indices.size());

        layer.vertices.insert(layer.vertices.end(), packed.begin(), packed.end());
        layer.indices.insert(layer.indices.end(), indices.begin(), indices.end());
        layer.faces.push_back(f);
    }

    return out;
}

} // namespace blockmodels

