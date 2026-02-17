#include <ASCIICraft/gui/GuiMeshes.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <vector>

namespace ASCIICraft::gui {

std::shared_ptr<ASCIIgL::Mesh> CreateQuadMesh() {
    using V = ASCIIgL::VertStructs::PosUV;

    std::vector<V> vertices;
    std::vector<int> indices;

    vertices.push_back({ -1.0f, -1.0f, 0.0f, 0.0f, 0.0f });
    vertices.push_back({ -1.0f,  1.0f, 0.0f, 0.0f, 1.0f });
    vertices.push_back({  1.0f,  1.0f, 0.0f, 1.0f, 1.0f });
    vertices.push_back({  1.0f, -1.0f, 0.0f, 1.0f, 0.0f });

    indices = { 0, 1, 2, 0, 2, 3 };

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    auto* texArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray").get();
    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUV(),
        std::move(indices),
        texArray
    );
}

} // namespace ASCIICraft::gui
