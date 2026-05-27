#include <ASCIICraft/util/QuadMeshBuilder.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <cstddef>
#include <vector>

namespace util {

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVQuad(ASCIIgL::Texture* texture) {
    if (!texture) return nullptr;

    using V = ASCIIgL::VertStructs::PosUV;
    std::vector<V> vertices(4);
    std::vector<int> indices{0, 1, 2, 0, 2, 3};

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

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVLayerQuad(ASCIIgL::Texture* texture, float layer) {
    if (!texture) return nullptr;

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
        texture
    );
}

std::shared_ptr<ASCIIgL::Mesh> QuadMeshBuilder::BuildPosUVLayerQuad(const std::shared_ptr<ASCIIgL::Texture>& texture, float layer) {
    return BuildPosUVLayerQuad(texture.get(), layer);
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