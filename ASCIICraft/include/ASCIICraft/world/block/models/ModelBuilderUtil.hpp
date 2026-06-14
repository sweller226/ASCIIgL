#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

namespace blockmodels::modelbuilderutil {

using V = ASCIIgL::VertStructs::PosUVLayer;

constexpr int FACE_COUNT = 6;
constexpr int VERTS_PER_FACE = 4;
constexpr int INDICES_PER_FACE = 6;

const std::array<glm::vec2, VERTS_PER_FACE>& GetFaceUVs();
const std::array<int, INDICES_PER_FACE>& GetFaceIndices();
const std::array<glm::vec3, VERTS_PER_FACE>& GetUnitFaceVerts(int faceIndex);

} // namespace blockmodels::modelbuilderutil
