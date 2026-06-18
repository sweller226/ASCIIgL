#include <ASCIICraft/util/QuadMeshBuilder.hpp>
#include <ASCIICraft/util/MeshBuilderUtil.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <array>
#include <vector>

namespace util {

glm::vec4 PixelRectToUV(float atlasSize, int x, int y, int w, int h) {
    const float minU = static_cast<float>(x) / atlasSize;
    const float maxU = static_cast<float>(x + w) / atlasSize;
    const float topV = static_cast<float>(y) / atlasSize;
    const float bottomV = static_cast<float>(y + h) / atlasSize;
    // uvRect = (minU, topV, maxU, bottomV) in top-left image space.
    return {minU, topV, maxU, bottomV};
}

namespace {

using PosUV = ASCIIgL::VertStructs::PosUV;
using PosUVLayer = ASCIIgL::VertStructs::PosUVLayer;
using PosColor = ASCIIgL::VertStructs::PosColor;

const std::array<glm::vec3, 4> kUnitQuadPositions = {{
    {-1.0f, -1.0f, 0.0f},
    {-1.0f,  1.0f, 0.0f},
    { 1.0f,  1.0f, 0.0f},
    { 1.0f, -1.0f, 0.0f},
}};

// Camera2D is Y-down, so model y=-1 maps to screen-top and y=+1 maps to screen-bottom.
// D3D11 texture V=0 is top row.
const std::array<glm::vec2, 4> kFullQuadUVs = {{
    {0.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f},
    {1.0f, 0.0f},
}};

std::shared_ptr<ASCIIgL::Mesh> MakePosUVMesh(
    std::vector<std::byte> byteVertices,
    std::vector<int> indices,
    ASCIIgL::Texture* texture
) {
    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUV(),
        std::move(indices),
        texture
    );
}

std::shared_ptr<ASCIIgL::Mesh> MakePosUVLayerMesh(
    std::vector<std::byte> byteVertices,
    std::vector<int> indices,
    ASCIIgL::TextureArray* textureArray
) {
    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices),
        textureArray
    );
}

} // namespace

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVQuad(ASCIIgL::Texture* texture) {
    if (!texture) return nullptr;

    std::vector<PosUV> vertices;
    std::vector<int> indices;
    AppendQuadPosUV(vertices, indices, kUnitQuadPositions, kFullQuadUVs);

    return MakePosUVMesh(PackVerts(vertices), std::move(indices), texture);
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVQuad(const std::shared_ptr<ASCIIgL::Texture>& texture) {
    return BuildPosUVQuad(texture.get());
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVQuad(ASCIIgL::Texture* texture, const glm::vec4& uvRect) {
    if (!texture) return nullptr;

    const float u0 = uvRect.x;
    const float vTop = uvRect.y;
    const float u1 = uvRect.z;
    const float vBottom = uvRect.w;

    const std::array<glm::vec2, 4> uvs = {{
        {u0, vTop},
        {u0, vBottom},
        {u1, vBottom},
        {u1, vTop},
    }};

    std::vector<PosUV> vertices;
    std::vector<int> indices;
    AppendQuadPosUV(vertices, indices, kUnitQuadPositions, uvs);

    return MakePosUVMesh(PackVerts(vertices), std::move(indices), texture);
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVQuad(const std::shared_ptr<ASCIIgL::Texture>& texture, const glm::vec4& uvRect) {
    return BuildPosUVQuad(texture.get(), uvRect);
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVLayerQuad(ASCIIgL::TextureArray* textureArray, float layer) {
    if (!textureArray) return nullptr;

    std::vector<PosUVLayer> vertices;
    std::vector<int> indices;
    AppendQuadPosUVLayer(vertices, indices, kUnitQuadPositions, kFullQuadUVs, layer);

    return MakePosUVLayerMesh(PackVerts(vertices), std::move(indices), textureArray);
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVLayerQuad(const std::shared_ptr<ASCIIgL::TextureArray>& textureArray, float layer) {
    return BuildPosUVLayerQuad(textureArray.get(), layer);
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosColorQuad(const glm::vec4& color) {
    std::array<PosColor, 4> corners{};
    for (int i = 0; i < 4; ++i) {
        corners[i].SetXYZ(kUnitQuadPositions[i]);
        corners[i].SetColor(color);
    }

    std::vector<PosColor> vertices;
    std::vector<int> indices;
    AppendQuad(vertices, indices, corners);

    return std::make_shared<ASCIIgL::Mesh>(
        PackVerts(vertices),
        ASCIIgL::VertFormats::PosColor(),
        std::move(indices)
    );
}

} // namespace util
