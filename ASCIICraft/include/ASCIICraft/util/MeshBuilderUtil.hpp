#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

namespace util {

inline void AppendQuadIndices(std::vector<int>& indices, int baseVertex) {
    indices.push_back(baseVertex + 0);
    indices.push_back(baseVertex + 1);
    indices.push_back(baseVertex + 2);
    indices.push_back(baseVertex + 0);
    indices.push_back(baseVertex + 2);
    indices.push_back(baseVertex + 3);
}

template<typename VertexT>
void AppendQuad(
    std::vector<VertexT>& vertices,
    std::vector<int>& indices,
    const std::array<VertexT, 4>& corners
) {
    const int base = static_cast<int>(vertices.size());
    for (const VertexT& corner : corners) {
        vertices.push_back(corner);
    }
    AppendQuadIndices(indices, base);
}

template<typename VertexT>
std::vector<std::byte> PackVerts(const std::vector<VertexT>& in) {
    const auto* begin = reinterpret_cast<const std::byte*>(in.data());
    const auto* end = begin + in.size() * sizeof(VertexT);
    return std::vector<std::byte>(begin, end);
}

void AppendQuadPosUV(
    std::vector<ASCIIgL::VertStructs::PosUV>& vertices,
    std::vector<int>& indices,
    const std::array<glm::vec3, 4>& positions,
    const std::array<glm::vec2, 4>& uvs
);

void AppendQuadPosUVLayer(
    std::vector<ASCIIgL::VertStructs::PosUVLayer>& vertices,
    std::vector<int>& indices,
    const std::array<glm::vec3, 4>& positions,
    const std::array<glm::vec2, 4>& uvs,
    float layer
);

} // namespace util
