#include <ASCIICraft/world/block/models/BlockModelBaker.hpp>
#include <ASCIICraft/world/block/models/ModelBuilderUtil.hpp>

#include <ASCIICraft/world/block/FaceCulling.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>
#include <ASCIICraft/world/block/textures/BlockTextureCatalog.hpp>

#include <array>
#include <cmath>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

namespace blockmodels {

namespace {

    using modelbuilderutil::V;

    constexpr float kModelCoordToBlock = 1.0f / 16.0f;

    bool IsValidQuarterTurn(int degrees) {
        return degrees == 0 || degrees == 90 || degrees == 180 || degrees == 270;
    }

    bool TryFaceIndex(const std::string& faceName, int& outFaceIndex) {
        if (faceName == "up") { outFaceIndex = static_cast<int>(FaceDir::Top); return true; }
        if (faceName == "down") { outFaceIndex = static_cast<int>(FaceDir::Bottom); return true; }
        if (faceName == "north") { outFaceIndex = static_cast<int>(FaceDir::North); return true; }
        if (faceName == "south") { outFaceIndex = static_cast<int>(FaceDir::South); return true; }
        if (faceName == "east") { outFaceIndex = static_cast<int>(FaceDir::East); return true; }
        if (faceName == "west") { outFaceIndex = static_cast<int>(FaceDir::West); return true; }
        return false;
    }

    std::array<glm::vec3, modelbuilderutil::VERTS_PER_FACE> BuildElementFaceVertsModelSpace(
        int faceIndex,
        const ResolvedBlockModelElement& elem
    ) {
        const float x1 = elem.from[0];
        const float y1 = elem.from[1];
        const float z1 = elem.from[2];
        const float x2 = elem.to[0];
        const float y2 = elem.to[1];
        const float z2 = elem.to[2];

        switch (faceIndex) {
            case static_cast<int>(FaceDir::Top):
                return { glm::vec3(x1, y2, z1), glm::vec3(x1, y2, z2), glm::vec3(x2, y2, z2), glm::vec3(x2, y2, z1) };
            case static_cast<int>(FaceDir::Bottom):
                return { glm::vec3(x1, y1, z2), glm::vec3(x1, y1, z1), glm::vec3(x2, y1, z1), glm::vec3(x2, y1, z2) };
            case static_cast<int>(FaceDir::North):
                return { glm::vec3(x1, y1, z2), glm::vec3(x2, y1, z2), glm::vec3(x2, y2, z2), glm::vec3(x1, y2, z2) };
            case static_cast<int>(FaceDir::South):
                return { glm::vec3(x2, y1, z1), glm::vec3(x1, y1, z1), glm::vec3(x1, y2, z1), glm::vec3(x2, y2, z1) };
            case static_cast<int>(FaceDir::East):
                return { glm::vec3(x2, y1, z2), glm::vec3(x2, y1, z1), glm::vec3(x2, y2, z1), glm::vec3(x2, y2, z2) };
            case static_cast<int>(FaceDir::West):
            default:
                return { glm::vec3(x1, y1, z1), glm::vec3(x1, y1, z2), glm::vec3(x1, y2, z2), glm::vec3(x1, y2, z1) };
        }
    }

    glm::vec3 RotateAxis(const glm::vec3& p, const glm::vec3& origin, char axis, float angleDeg) {
        const float r = angleDeg * (3.1415926535f / 180.0f);
        const float c = std::cos(r);
        const float s = std::sin(r);
        const glm::vec3 t = p - origin;
        glm::vec3 out = t;
        switch (axis) {
            case 'x': out = glm::vec3(t.x, t.y * c - t.z * s, t.y * s + t.z * c); break;
            case 'y': out = glm::vec3(t.x * c - t.z * s, t.y, t.x * s + t.z * c); break;
            case 'z': out = glm::vec3(t.x * c - t.y * s, t.x * s + t.y * c, t.z); break;
            default: break;
        }
        return out + origin;
    }

    glm::vec3 ApplyElementRotationModelSpace(const glm::vec3& p, const ResolvedBlockModelElement& elem) {
        if (!elem.rotation.has_value()) return p;
        const auto& r = elem.rotation.value();
        const glm::vec3 origin(r.origin[0], r.origin[1], r.origin[2]);
        glm::vec3 out = RotateAxis(p, origin, r.axis, r.angle);
        if (r.rescale) {
            // Approximate vanilla rescale behavior: scale around origin to reduce clipping
            // for non-right-angle element rotations.
            glm::vec3 o = origin;
            glm::vec3 d = out - o;
            float k = 1.0f;
            if (r.axis == 'x') k = 1.0f / std::max(0.0001f, std::abs(std::cos(r.angle * (3.1415926535f / 180.0f))));
            if (r.axis == 'y') k = 1.0f / std::max(0.0001f, std::abs(std::cos(r.angle * (3.1415926535f / 180.0f))));
            if (r.axis == 'z') k = 1.0f / std::max(0.0001f, std::abs(std::cos(r.angle * (3.1415926535f / 180.0f))));
            if (r.axis == 'x') d = glm::vec3(d.x, d.y * k, d.z * k);
            if (r.axis == 'y') d = glm::vec3(d.x * k, d.y, d.z * k);
            if (r.axis == 'z') d = glm::vec3(d.x * k, d.y * k, d.z);
            out = o + d;
        }
        return out;
    }

    glm::vec3 ApplyVariantRotationsBlockSpace(const glm::vec3& p, int variantX, int variantY) {
        const glm::vec3 c(0.5f, 0.5f, 0.5f);
        glm::vec3 out = p;
        if (variantX != 0) {
            out = RotateAxis(out, c, 'x', static_cast<float>(variantX));
        }
        if (variantY != 0) {
            out = RotateAxis(out, c, 'y', static_cast<float>(variantY));
        }
        return out;
    }

    /// Outward unit normal for a full cube face in model space (matches \ref BuildElementFaceVertsModelSpace).
    glm::vec3 OutwardNormalForFaceIndex(int faceIndex) {
        switch (faceIndex) {
            case static_cast<int>(FaceDir::Top): return glm::vec3(0.0f, 1.0f, 0.0f);
            case static_cast<int>(FaceDir::Bottom): return glm::vec3(0.0f, -1.0f, 0.0f);
            case static_cast<int>(FaceDir::North): return glm::vec3(0.0f, 0.0f, 1.0f);
            case static_cast<int>(FaceDir::South): return glm::vec3(0.0f, 0.0f, -1.0f);
            case static_cast<int>(FaceDir::East): return glm::vec3(1.0f, 0.0f, 0.0f);
            case static_cast<int>(FaceDir::West): return glm::vec3(-1.0f, 0.0f, 0.0f);
            default: return glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    /// Same X-then-Y rotations as \ref ApplyVariantRotationsBlockSpace, but for direction vectors (no translation).
    glm::vec3 ApplyVariantRotationToNormal(glm::vec3 n, int variantX, int variantY) {
        const glm::vec3 origin(0.0f);
        if (variantX != 0) {
            n = RotateAxis(n, origin, 'x', static_cast<float>(variantX));
        }
        if (variantY != 0) {
            n = RotateAxis(n, origin, 'y', static_cast<float>(variantY));
        }
        return n;
    }

    /// Element JSON \c rotation affects vertices via \ref ApplyElementRotationModelSpace; outward normals transform
    /// by the same axis/angle (pivot does not matter for directions). The optional rescale path skews positions only —
    /// we do not apply that to normals (neighbor culling stays axis-aligned dominant for typical 22.5°/45° models).
    glm::vec3 ApplyElementRotationToNormal(glm::vec3 n, const ResolvedBlockModelElement& elem) {
        if (!elem.rotation.has_value()) {
            return n;
        }
        const glm::vec3 origin(0.0f);
        const auto& r = elem.rotation.value();
        return RotateAxis(n, origin, r.axis, r.angle);
    }

    /// After variant rotation, which world-aligned block face (+Y top, +Z north, …) does this normal correspond to?
    int FaceIndexFromOutwardNormal(const glm::vec3& n) {
        const glm::vec3 a = glm::normalize(n);
        const float ax = std::abs(a.x);
        const float ay = std::abs(a.y);
        const float az = std::abs(a.z);
        if (ax >= ay && ax >= az) {
            return a.x > 0.0f ? static_cast<int>(FaceDir::East) : static_cast<int>(FaceDir::West);
        }
        if (ay >= ax && ay >= az) {
            return a.y > 0.0f ? static_cast<int>(FaceDir::Top) : static_cast<int>(FaceDir::Bottom);
        }
        return a.z > 0.0f ? static_cast<int>(FaceDir::North) : static_cast<int>(FaceDir::South);
    }

    /// Maps JSON face name to neighbor-culling slot after element rotation then blockstate \c x / \c y variant rotation.
    int RemapCardinalFaceForBake(
        int modelFaceIndex,
        const ResolvedBlockModelElement& elem,
        int variantX,
        int variantY
    ) {
        glm::vec3 n = OutwardNormalForFaceIndex(modelFaceIndex);
        n = ApplyElementRotationToNormal(n, elem);
        n = ApplyVariantRotationToNormal(n, variantX, variantY);
        return FaceIndexFromOutwardNormal(n);
    }

    std::array<glm::vec2, modelbuilderutil::VERTS_PER_FACE> BuildDefaultFaceUVs(
        int faceIndex,
        const ResolvedBlockModelElement& elem
    ) {
        const float x1 = elem.from[0] * kModelCoordToBlock;
        const float y1 = elem.from[1] * kModelCoordToBlock;
        const float z1 = elem.from[2] * kModelCoordToBlock;
        const float x2 = elem.to[0] * kModelCoordToBlock;
        const float y2 = elem.to[1] * kModelCoordToBlock;
        const float z2 = elem.to[2] * kModelCoordToBlock;

        float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
        switch (faceIndex) {
            case static_cast<int>(FaceDir::Top):
            case static_cast<int>(FaceDir::Bottom):
                u0 = x1; v0 = z1; u1 = x2; v1 = z2;
                break;
            case static_cast<int>(FaceDir::North):
            case static_cast<int>(FaceDir::South):
                u0 = x1; v0 = 1.0f - y2; u1 = x2; v1 = 1.0f - y1;
                break;
            case static_cast<int>(FaceDir::East):
            case static_cast<int>(FaceDir::West):
                u0 = z1; v0 = 1.0f - y2; u1 = z2; v1 = 1.0f - y1;
                break;
            default:
                break;
        }
        return { glm::vec2(u0, v0), glm::vec2(u1, v0), glm::vec2(u1, v1), glm::vec2(u0, v1) };
    }

    std::array<glm::vec2, modelbuilderutil::VERTS_PER_FACE> BuildFaceUVs(
        int faceIndex,
        const ResolvedBlockModelElement& elem,
        const ResolvedBlockModelFace& face
    ) {
        if (!face.uv.has_value()) {
            return BuildDefaultFaceUVs(faceIndex, elem);
        }
        const auto& uv = face.uv.value();
        const float u0 = uv[0] * kModelCoordToBlock;
        const float v0 = uv[1] * kModelCoordToBlock;
        const float u1 = uv[2] * kModelCoordToBlock;
        const float v1 = uv[3] * kModelCoordToBlock;
        return { glm::vec2(u0, v0), glm::vec2(u1, v0), glm::vec2(u1, v1), glm::vec2(u0, v1) };
    }

    std::array<glm::vec2, modelbuilderutil::VERTS_PER_FACE> RotateFaceUVs(
        const std::array<glm::vec2, modelbuilderutil::VERTS_PER_FACE>& in,
        int faceIndex,
        int rotationDegrees
    ) {
        std::array<glm::vec2, modelbuilderutil::VERTS_PER_FACE> out = in;
        int steps = (rotationDegrees / 90) & 3;
        while (steps-- > 0) {
            if (faceIndex == static_cast<int>(FaceDir::Bottom)) {
                // Vanilla parity: "down" face rotation is counterclockwise.
                out = { out[1], out[2], out[3], out[0] };
            } else {
                out = { out[3], out[0], out[1], out[2] }; // 90° clockwise
            }
        }
        return out;
    }

    std::array<glm::vec2, modelbuilderutil::VERTS_PER_FACE> ApplyUvlock(
        const std::array<glm::vec2, modelbuilderutil::VERTS_PER_FACE>& in,
        int faceIndex,
        int variantX,
        int variantY,
        bool uvlock
    ) {
        if (!uvlock) return in;

        auto rotateFaceY = [](int f) -> int {
            switch (f) {
                case static_cast<int>(FaceDir::North): return static_cast<int>(FaceDir::East);
                case static_cast<int>(FaceDir::East): return static_cast<int>(FaceDir::South);
                case static_cast<int>(FaceDir::South): return static_cast<int>(FaceDir::West);
                case static_cast<int>(FaceDir::West): return static_cast<int>(FaceDir::North);
                default: return f;
            }
        };
        auto rotateFaceX = [](int f) -> int {
            switch (f) {
                case static_cast<int>(FaceDir::Top): return static_cast<int>(FaceDir::South);
                case static_cast<int>(FaceDir::South): return static_cast<int>(FaceDir::Bottom);
                case static_cast<int>(FaceDir::Bottom): return static_cast<int>(FaceDir::North);
                case static_cast<int>(FaceDir::North): return static_cast<int>(FaceDir::Top);
                default: return f; // east/west unchanged around X
            }
        };

        int transformedFace = faceIndex;
        int ySteps = ((variantY / 90) % 4 + 4) % 4;
        int xSteps = ((variantX / 90) % 4 + 4) % 4;
        while (xSteps-- > 0) transformedFace = rotateFaceX(transformedFace);
        while (ySteps-- > 0) transformedFace = rotateFaceY(transformedFace);

        // Better uvlock rule than previous heuristic:
        // counter-rotate yaw for all faces, and add pitch compensation for east/west-facing sides.
        // This aligns common fence/stair/torch variant behavior with world-facing texture orientation.
        int correction = (360 - variantY) % 360;
        if (transformedFace == static_cast<int>(FaceDir::East) || transformedFace == static_cast<int>(FaceDir::West)) {
            correction = (correction + (360 - variantX) % 360) % 360;
        }
        return RotateFaceUVs(in, faceIndex, correction);
    }

} // namespace

jsonutil::LoadResult<blockstate::BlockModel> BakeResolvedModel(
    const ResolvedBlockModelDefinition& resolved,
    int variantX,
    int variantY,
    bool variantUvlock
) {
    if (!IsValidQuarterTurn(variantX)) {
        return jsonutil::Fail<blockstate::BlockModel>("BakeResolvedModel: variant.x must be one of {0,90,180,270}");
    }
    if (!IsValidQuarterTurn(variantY)) {
        return jsonutil::Fail<blockstate::BlockModel>("BakeResolvedModel: variant.y must be one of {0,90,180,270}");
    }

    blockstate::BlockModel out;
    out.isFullBlock = resolved.isFullBlock;
    out.opaqueNoCull = resolved.opaqueNoCull;
    out.computeVisibleFaces = out.isFullBlock ? faceculling::ComputeVisibleFacesFullBlock : nullptr;

    blockstate::RenderLayer& layer = out.opaque; // JSON path defaults to opaque for now.
    layer.faces.reserve(resolved.elements.size() * modelbuilderutil::FACE_COUNT);

    const auto& faceIndices = modelbuilderutil::GetFaceIndices();
    for (const auto& elem : resolved.elements) {
        for (const auto& faceKv : elem.faces) {
            int faceIndex = 0;
            if (!TryFaceIndex(faceKv.first, faceIndex)) {
                continue;
            }
            const auto& face = faceKv.second;
            const int layerIdx = blocktextures::GetLayerForTextureId(face.texture);
            if (layerIdx < 0) {
                return jsonutil::Fail<blockstate::BlockModel>(
                    "BakeResolvedModel: missing texture catalog entry for '" + face.texture + "'"
                );
            }

            const float texLayer = static_cast<float>(layerIdx);
            auto vertsPosModel = BuildElementFaceVertsModelSpace(faceIndex, elem);
            auto faceUVs = BuildFaceUVs(faceIndex, elem, face);
            faceUVs = RotateFaceUVs(faceUVs, faceIndex, face.rotation);
            faceUVs = ApplyUvlock(faceUVs, faceIndex, variantX, variantY, variantUvlock);

            const int vertByteOffset = static_cast<int>(layer.vertices.size());
            const int idxOffset = static_cast<int>(layer.indices.size());

            std::vector<V> packedVerts;
            packedVerts.reserve(modelbuilderutil::VERTS_PER_FACE);
            for (int i = 0; i < modelbuilderutil::VERTS_PER_FACE; ++i) {
                // Transform stack (M3C):
                // 1) element rotation in model-space
                // 2) normalize 0..16 to 0..1 block space
                // 3) variant x/y rotation in block space
                glm::vec3 p = ApplyElementRotationModelSpace(vertsPosModel[i], elem);
                p *= kModelCoordToBlock;
                p = ApplyVariantRotationsBlockSpace(p, variantX, variantY);

                V v{};
                v.SetXYZ(p);
                v.SetUV(glm::vec2(faceUVs[i].x, 1.0f - faceUVs[i].y)); // match existing V flip convention
                v.SetLayer(texLayer);
                v.SetLight(1.0f);
                packedVerts.push_back(v);
            }

            std::vector<std::byte> vertBytes = modelbuilderutil::PackVerts(packedVerts);
            layer.vertices.insert(layer.vertices.end(), vertBytes.begin(), vertBytes.end());
            for (int i = 0; i < modelbuilderutil::INDICES_PER_FACE; ++i) {
                // Keep indices face-local (0..3). Append paths rebase with per-face destination base vertex.
                layer.indices.push_back(faceIndices[i]);
            }

            blockstate::FaceRange faceRange{};
            faceRange.vertByteOffset = vertByteOffset;
            faceRange.vertByteCount = static_cast<int>(vertBytes.size());
            faceRange.idxOffset = idxOffset;
            faceRange.idxCount = modelbuilderutil::INDICES_PER_FACE;
            faceRange.cardinalFace = static_cast<uint8_t>(
                RemapCardinalFaceForBake(faceIndex, elem, variantX, variantY)
            );
            layer.faces.push_back(faceRange);
        }
    }

    return jsonutil::LoadResult<blockstate::BlockModel>::Success(std::move(out));
}

} // namespace blockmodels

