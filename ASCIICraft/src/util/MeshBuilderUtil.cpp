#include <ASCIICraft/util/MeshBuilderUtil.hpp>

namespace util {

void AppendQuadPosUV(
    std::vector<ASCIIgL::VertStructs::PosUV>& vertices,
    std::vector<int>& indices,
    const std::array<glm::vec3, 4>& positions,
    const std::array<glm::vec2, 4>& uvs
) {
    using V = ASCIIgL::VertStructs::PosUV;
    std::array<V, 4> corners{};
    for (int i = 0; i < 4; ++i) {
        corners[i].SetXYZ(positions[i]);
        corners[i].SetUV(uvs[i]);
    }
    AppendQuad(vertices, indices, corners);
}

void AppendQuadPosUVLayer(
    std::vector<ASCIIgL::VertStructs::PosUVLayer>& vertices,
    std::vector<int>& indices,
    const std::array<glm::vec3, 4>& positions,
    const std::array<glm::vec2, 4>& uvs,
    float layer
) {
    using V = ASCIIgL::VertStructs::PosUVLayer;
    std::array<V, 4> corners{};
    for (int i = 0; i < 4; ++i) {
        corners[i].SetXYZ(positions[i]);
        corners[i].SetUV(uvs[i]);
        corners[i].SetLayer(layer);
    }
    AppendQuad(vertices, indices, corners);
}

} // namespace util
