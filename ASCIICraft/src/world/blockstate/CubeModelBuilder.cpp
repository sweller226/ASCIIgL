#include <ASCIICraft/world/blockstate/CubeModelBuilder.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

#include <array>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>

namespace blockstate {

namespace {

using V = ASCIIgL::VertStructs::PosUVLayerLight;

static constexpr int kFaceCount = 6;

inline glm::vec2 RotateTopBottomUVLocal(glm::vec2 uv, FaceDir facing) {
    float u = uv.x, v = uv.y;
    switch (facing) {
        case FaceDir::North: return uv;                           // 0°
        case FaceDir::South: return glm::vec2(1.0f - u, 1.0f - v); // 180°
        case FaceDir::East:  return glm::vec2(1.0f - v, u);        // 90° CW
        case FaceDir::West:  return glm::vec2(v, 1.0f - u);        // 270° CW
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

static const glm::vec2 kFaceUVs[4] = {
    glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)
};

static const int kFaceIndices[6] = { 0, 1, 2, 0, 2, 3 };

inline void AppendFace(
    const CubeSpec& spec,
    int worldFaceIndex,
    std::vector<V>& verts,
    std::vector<int>& indices
) {
    // Local-space unit cube corners for each face (same winding/order as ChunkMeshGen).
    static const glm::vec3 unitFaceVerts[kFaceCount][4] = {
        // Top (+Y)
        { glm::vec3(0, 1, 0), glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 0) },
        // Bottom (-Y)
        { glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(1, 0, 1) },
        // North (+Z)
        { glm::vec3(0, 0, 1), glm::vec3(1, 0, 1), glm::vec3(1, 1, 1), glm::vec3(0, 1, 1) },
        // South (-Z)
        { glm::vec3(1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 0) },
        // East (+X)
        { glm::vec3(1, 0, 1), glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(1, 1, 1) },
        // West (-X)
        { glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), glm::vec3(0, 1, 0) },
    };

    const int sourceFaceIndex = SourceFaceForWorldFace(worldFaceIndex, spec.facing);
    const int layer = spec.faceLayers[sourceFaceIndex];
    const float light = 1.0f;

    const int startIndex = static_cast<int>(verts.size());

    for (int vi = 0; vi < 4; ++vi) {
        const glm::vec3 pos = unitFaceVerts[worldFaceIndex][vi];

        glm::vec2 uv = kFaceUVs[vi];
        if (worldFaceIndex == static_cast<int>(FaceDir::Top) || worldFaceIndex == static_cast<int>(FaceDir::Bottom)) {
            uv = RotateTopBottomUVLocal(uv, spec.facing);
        }

        // Match ChunkMeshGen: flip V.
        const float u = uv.x;
        const float v = 1.0f - uv.y;

        V vert{};
        vert.SetXYZ(pos);
        vert.SetUV(glm::vec2(u, v));
        vert.SetLayer(static_cast<float>(layer));
        vert.SetLight(light);
        verts.push_back(vert);
    }

    for (int i = 0; i < 6; ++i) {
        indices.push_back(startIndex + kFaceIndices[i]);
    }
}

inline std::vector<std::byte> PackVerts(const std::vector<V>& in) {
    const auto* begin = reinterpret_cast<const std::byte*>(in.data());
    const auto* end = begin + (in.size() * sizeof(V));
    return std::vector<std::byte>(begin, end);
}

} // namespace

BlockModel BuildCubeModel(const CubeSpec& spec) {
    BlockModel out;
    out.fullBlock = true;

    std::vector<V> verts;
    std::vector<int> indices;
    verts.reserve(static_cast<size_t>(kFaceCount) * 4);
    indices.reserve(static_cast<size_t>(kFaceCount) * 6);

    for (int face = 0; face < kFaceCount; ++face) {
        AppendFace(spec, face, verts, indices);
    }

    if (spec.transparent) {
        out.transparentVertices = PackVerts(verts);
        out.transparentIndices = std::move(indices);
    } else {
        out.opaqueVertices = PackVerts(verts);
        out.opaqueIndices = std::move(indices);
    }

    return out;
}

} // namespace blockstate

