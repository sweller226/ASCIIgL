#include <ASCIICraft/util/QuadMeshBuilder.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <cstddef>
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

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVQuad(ASCIIgL::Texture* texture) {
    if (!texture) return nullptr;

    using V = ASCIIgL::VertStructs::PosUV;
    std::vector<V> vertices(4);
    std::vector<int> indices{0, 1, 2, 0, 2, 3};

    // Camera2D is Y-down, so model y=-1 maps to screen-top and y=+1 maps to screen-bottom.
    // D3D11 texture V=0 is top row.
    vertices[0].SetXYZ({-1.0f, -1.0f, 0.0f}); vertices[0].SetUV({0.0f, 0.0f});
    vertices[1].SetXYZ({-1.0f,  1.0f, 0.0f}); vertices[1].SetUV({0.0f, 1.0f});
    vertices[2].SetXYZ({ 1.0f,  1.0f, 0.0f}); vertices[2].SetUV({1.0f, 1.0f});
    vertices[3].SetXYZ({ 1.0f, -1.0f, 0.0f}); vertices[3].SetUV({1.0f, 0.0f});

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUV(),
        std::move(indices),
        texture
    );
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

    using V = ASCIIgL::VertStructs::PosUV;
    std::vector<V> vertices(4);
    std::vector<int> indices{0, 1, 2, 0, 2, 3};

    vertices[0].SetXYZ({-1.0f, -1.0f, 0.0f}); vertices[0].SetUV({u0, vTop});
    vertices[1].SetXYZ({-1.0f,  1.0f, 0.0f}); vertices[1].SetUV({u0, vBottom});
    vertices[2].SetXYZ({ 1.0f,  1.0f, 0.0f}); vertices[2].SetUV({u1, vBottom});
    vertices[3].SetXYZ({ 1.0f, -1.0f, 0.0f}); vertices[3].SetUV({u1, vTop});

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUV(),
        std::move(indices),
        texture
    );
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVQuad(const std::shared_ptr<ASCIIgL::Texture>& texture, const glm::vec4& uvRect) {
    return BuildPosUVQuad(texture.get(), uvRect);
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVLayerQuad(ASCIIgL::TextureArray* textureArray, float layer) {
    if (!textureArray) return nullptr;

    using V = ASCIIgL::VertStructs::PosUVLayer;
    std::vector<V> vertices(4);
    std::vector<int> indices{0, 1, 2, 0, 2, 3};

    vertices[0].SetXYZ({-1.0f, -1.0f, 0.0f}); vertices[0].SetUV({0.0f, 0.0f}); vertices[0].SetLayer(layer);
    vertices[1].SetXYZ({-1.0f,  1.0f, 0.0f}); vertices[1].SetUV({0.0f, 1.0f}); vertices[1].SetLayer(layer);
    vertices[2].SetXYZ({ 1.0f,  1.0f, 0.0f}); vertices[2].SetUV({1.0f, 1.0f}); vertices[2].SetLayer(layer);
    vertices[3].SetXYZ({ 1.0f, -1.0f, 0.0f}); vertices[3].SetUV({1.0f, 0.0f}); vertices[3].SetLayer(layer);

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices),
        textureArray
    );
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVLayerQuad(const std::shared_ptr<ASCIIgL::TextureArray>& textureArray, float layer) {
    return BuildPosUVLayerQuad(textureArray.get(), layer);
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosColorQuad(const glm::vec4& color) {
    using V = ASCIIgL::VertStructs::PosColor;
    std::vector<V> vertices(4);
    std::vector<int> indices{0, 1, 2, 0, 2, 3};

    vertices[0].SetXYZ({-1.0f, -1.0f, 0.0f}); vertices[0].SetColor(color);
    vertices[1].SetXYZ({-1.0f,  1.0f, 0.0f}); vertices[1].SetColor(color);
    vertices[2].SetXYZ({ 1.0f,  1.0f, 0.0f}); vertices[2].SetColor(color);
    vertices[3].SetXYZ({ 1.0f, -1.0f, 0.0f}); vertices[3].SetColor(color);

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosColor(),
        std::move(indices)
    );
}

} // namespace util