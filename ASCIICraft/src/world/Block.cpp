#include <ASCIICraft/world/Block.hpp>

#include <ASCIIgL/util/Logger.hpp>

// Static texture array definition
ASCIIgL::TextureArray* Block::blockTextureArray = nullptr;

// Get texture layer for a block face
int Block::GetTextureLayer(BlockFace face) const {
    return BlockTextures::GetBlockFaceLayer(type, face);
}

// Static texture array management
void Block::SetTextureArray(ASCIIgL::TextureArray* texArray) {
    blockTextureArray = texArray;
}

ASCIIgL::TextureArray* Block::GetTextureArray() {
    return blockTextureArray;
}

bool Block::HasTextureArray() {
    return blockTextureArray != nullptr;
}

namespace BlockTextures {
    // Atlas dimensions (terrain.png is 16x16 tiles)
    constexpr int ATLAS_TILES = 16;

    // Get texture layer for block faces based on Minecraft Beta 1.7.3 terrain.png
    int GetBlockFaceLayer(BlockType blockType, BlockFace face)
    {
        switch (blockType)
        {
            // === Terrain ===
            case BlockType::Bedrock:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(1, 1, ATLAS_TILES);
            case BlockType::Stone:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(1, 0, ATLAS_TILES);

            case BlockType::Cobblestone:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(0, 1, ATLAS_TILES);

            case BlockType::Dirt:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(2, 0, ATLAS_TILES);

            case BlockType::Grass:
                switch (face) {
                    case BlockFace::Top:    return ASCIIgL::TextureArray::GetLayerFromAtlasXY(0, 0, ATLAS_TILES);
                    case BlockFace::Bottom: return ASCIIgL::TextureArray::GetLayerFromAtlasXY(2, 0, ATLAS_TILES);
                    default:                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(3, 0, ATLAS_TILES);
                }

            case BlockType::Gravel:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(3, 1, ATLAS_TILES);

            case BlockType::Sand:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(2, 1, ATLAS_TILES);

            case BlockType::Sandstone:
                switch (face) {
                    case BlockFace::Top:    return ASCIIgL::TextureArray::GetLayerFromAtlasXY(2, 2, ATLAS_TILES);
                    case BlockFace::Bottom: return ASCIIgL::TextureArray::GetLayerFromAtlasXY(2, 2, ATLAS_TILES);
                    default:                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(2, 2, ATLAS_TILES);
                }

            case BlockType::Clay:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(4, 1, ATLAS_TILES);


            // === Ores ===
            case BlockType::Coal_Ore:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(2, 2, ATLAS_TILES);

            case BlockType::Iron_Ore:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(1, 2, ATLAS_TILES);

            case BlockType::Gold_Ore:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(0, 2, ATLAS_TILES);

            case BlockType::Diamond_Ore:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(3, 2, ATLAS_TILES);


            // === Wood & Plants ===
            case BlockType::Oak_Log:
                switch (face) {
                    case BlockFace::Top:
                    case BlockFace::Bottom:
                        return ASCIIgL::TextureArray::GetLayerFromAtlasXY(5, 1, ATLAS_TILES); // rings
                    default:
                        return ASCIIgL::TextureArray::GetLayerFromAtlasXY(4, 1, ATLAS_TILES); // bark
                }

            case BlockType::Oak_Leaves:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(4, 3, ATLAS_TILES);

            case BlockType::Oak_Planks:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(4, 0, ATLAS_TILES);

            case BlockType::Spruce_Log:
                switch (face) {
                    case BlockFace::Top:
                    case BlockFace::Bottom:
                        return ASCIIgL::TextureArray::GetLayerFromAtlasXY(5, 1, ATLAS_TILES);
                    default:
                        return ASCIIgL::TextureArray::GetLayerFromAtlasXY(4, 1, ATLAS_TILES);
                }

            case BlockType::Spruce_Leaves:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(5, 3, ATLAS_TILES);

            case BlockType::Spruce_Planks:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(5, 0, ATLAS_TILES);


            // === Utility Blocks ===
            case BlockType::Crafting_Table:
                switch (face) {
                    case BlockFace::Top:    return ASCIIgL::TextureArray::GetLayerFromAtlasXY(11, 2, ATLAS_TILES);
                    case BlockFace::Bottom: return ASCIIgL::TextureArray::GetLayerFromAtlasXY(4, 0, ATLAS_TILES);
                    case BlockFace::North:  return ASCIIgL::TextureArray::GetLayerFromAtlasXY(11, 3, ATLAS_TILES);
                    case BlockFace::South:  return ASCIIgL::TextureArray::GetLayerFromAtlasXY(11, 3, ATLAS_TILES);
                    case BlockFace::East:   return ASCIIgL::TextureArray::GetLayerFromAtlasXY(12, 3, ATLAS_TILES);
                    case BlockFace::West:   return ASCIIgL::TextureArray::GetLayerFromAtlasXY(12, 3, ATLAS_TILES);
                }
                break;

            case BlockType::Furnace:
                switch (face) {
                    case BlockFace::Top:
                    case BlockFace::Bottom:
                        return ASCIIgL::TextureArray::GetLayerFromAtlasXY(14, 3, ATLAS_TILES);
                    case BlockFace::North:  return ASCIIgL::TextureArray::GetLayerFromAtlasXY(12, 2, ATLAS_TILES); // front
                    default:                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(13, 2, ATLAS_TILES); // sides/back
                }


            // === Special Blocks ===
            case BlockType::TNT:
                switch (face) {
                    case BlockFace::Top:    return ASCIIgL::TextureArray::GetLayerFromAtlasXY(8, 0, ATLAS_TILES);
                    case BlockFace::Bottom: return ASCIIgL::TextureArray::GetLayerFromAtlasXY(8, 0, ATLAS_TILES);
                    default:                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(8, 0, ATLAS_TILES);
                }

            case BlockType::Obsidian:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(5, 2, ATLAS_TILES);

            case BlockType::Mossy_Cobblestone:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(0, 3, ATLAS_TILES);

            case BlockType::Bookshelf:
                switch (face) {
                    case BlockFace::Top:
                    case BlockFace::Bottom:
                        return ASCIIgL::TextureArray::GetLayerFromAtlasXY(4, 0, ATLAS_TILES);
                    default:
                        return ASCIIgL::TextureArray::GetLayerFromAtlasXY(3, 3, ATLAS_TILES);
                }

            case BlockType::Wool:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(0, 4, ATLAS_TILES);

            // === Default / Unknown ===
            default:
                return ASCIIgL::TextureArray::GetLayerFromAtlasXY(15, 1, ATLAS_TILES);
        }
    }


    std::shared_ptr<ASCIIgL::Mesh> GetBlockMesh(BlockType blockType) {
        using V = ASCIIgL::VertStructs::PosUVLayer;

        std::vector<V> vertices;
        std::vector<int> indices;

        auto pushFace = [&](BlockFace face,
                            const glm::vec3& v0,
                            const glm::vec3& v1,
                            const glm::vec3& v2,
                            const glm::vec3& v3)
        {
            int layer = GetBlockFaceLayer(blockType, face);

            // Quad vertices (CCW)
            int startIndex = vertices.size();

            vertices.push_back({ v0.x, v0.y, v0.z, 0.0f, 0.0f, (float)layer });
            vertices.push_back({ v1.x, v1.y, v1.z, 0.0f, 1.0f, (float)layer });
            vertices.push_back({ v2.x, v2.y, v2.z, 1.0f, 1.0f, (float)layer });
            vertices.push_back({ v3.x, v3.y, v3.z, 1.0f, 0.0f, (float)layer });

            // Two triangles
            indices.push_back(startIndex + 0);
            indices.push_back(startIndex + 1);
            indices.push_back(startIndex + 2);

            indices.push_back(startIndex + 0);
            indices.push_back(startIndex + 2);
            indices.push_back(startIndex + 3);
        };

        // Cube corners
        glm::vec3 p000(-1, -1, -1);
        glm::vec3 p001(-1, -1,  1);
        glm::vec3 p010(-1,  1, -1);
        glm::vec3 p011(-1,  1,  1);
        glm::vec3 p100( 1, -1, -1);
        glm::vec3 p101( 1, -1,  1);
        glm::vec3 p110( 1,  1, -1);
        glm::vec3 p111( 1,  1,  1);

        // Top (+Y)
        pushFace(BlockFace::Top,    p011, p111, p110, p010);

        // Bottom (-Y)
        pushFace(BlockFace::Bottom, p001, p000, p100, p101);

        // North (+Z)
        pushFace(BlockFace::North,  p001, p011, p111, p101);

        // South (-Z)
        pushFace(BlockFace::South,  p100, p110, p010, p000);

        // East (+X)
        pushFace(BlockFace::East,   p101, p111, p110, p100);

        // West (-X)
        pushFace(BlockFace::West,   p000, p010, p011, p001);

        // Convert vertices to byte buffer
        std::vector<std::byte> byteVertices(
            reinterpret_cast<std::byte*>(vertices.data()),
            reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
        );

        return std::make_shared<ASCIIgL::Mesh>(
            std::move(byteVertices),
            ASCIIgL::VertFormats::PosUVLayer(),
            std::move(indices),
            Block::GetTextureArray()
        );
    }
}