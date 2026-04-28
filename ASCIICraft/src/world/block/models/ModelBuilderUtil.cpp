#include <ASCIICraft/world/block/models/ModelBuilderUtil.hpp>

namespace blockmodels::modelbuilderutil {

const std::array<glm::vec2, VERTS_PER_FACE>& GetFaceUVs() {
    static const std::array<glm::vec2, VERTS_PER_FACE> kFaceUVs = {
        glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)
    };
    return kFaceUVs;
}

const std::array<int, INDICES_PER_FACE>& GetFaceIndices() {
    static const std::array<int, INDICES_PER_FACE> kFaceIndices = { 0, 1, 2, 0, 2, 3 };
    return kFaceIndices;
}

const std::array<glm::vec3, VERTS_PER_FACE>& GetUnitFaceVerts(int faceIndex) {
    static const std::array<std::array<glm::vec3, VERTS_PER_FACE>, FACE_COUNT> kUnitFaceVerts = {{
        // Top (+Y)
        { glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f) },
        // Bottom (-Y)
        { glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 1.0f) },
        // North (+Z)
        { glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f) },
        // South (-Z)
        { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f) },
        // East (+X)
        { glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f) },
        // West (-X)
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f) },
    }};
    return kUnitFaceVerts[faceIndex];
}

std::vector<std::byte> PackVerts(const std::vector<V>& in) {
    const auto* begin = reinterpret_cast<const std::byte*>(in.data());
    const auto* end = begin + (in.size() * sizeof(V));
    return std::vector<std::byte>(begin, end);
}

} // namespace blockmodels::modelbuilderutil
