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
    
    // Convert tile coordinates to layer index
    int GetTileLayer(int tileX, int tileY) {
        int layer = tileY * ATLAS_TILES + tileX;
        return layer;
    }

    // Get texture layer for block faces based on Minecraft Beta 1.7.3 terrain.png
    int GetBlockFaceLayer(BlockType blockType, BlockFace face) {
        switch (blockType) {
            case BlockType::Stone:
                return GetTileLayer(1, 0);  // Stone (all faces same)
                
            case BlockType::Dirt:
                return GetTileLayer(2, 0);  // Dirt (all faces same)
                
            case BlockType::Grass:
                switch (face) {
                    case BlockFace::Top:    return GetTileLayer(0, 0);  // Grass top
                    case BlockFace::Bottom: return GetTileLayer(2, 0);  // Dirt bottom
                    default:                return GetTileLayer(3, 0);  // Grass side
                }
                
            case BlockType::Wood:
                switch (face) {
                    case BlockFace::Top:    return GetTileLayer(5, 1);  // Wood rings
                    case BlockFace::Bottom: return GetTileLayer(5, 1);
                    default:                return GetTileLayer(4, 1);  // Wood bark
                }
                
            case BlockType::Leaves:
                return GetTileLayer(4, 3);
                
            case BlockType::Gravel:
                return GetTileLayer(3, 1);
                
            case BlockType::Coal_Ore:
                return GetTileLayer(2, 2);
                
            case BlockType::Iron_Ore:
                return GetTileLayer(1, 2);

            case BlockType::Diamond_Ore:
                return GetTileLayer(0, 2);
                
            case BlockType::Bedrock:
                return GetTileLayer(1, 1);

            case BlockType::Cobblestone:
                return GetTileLayer(0, 1);

            case BlockType::Crafting_Table:
                switch (face) {
                    case BlockFace::Top:    return GetTileLayer(11, 2);
                    case BlockFace::Bottom: return GetTileLayer(4, 0);
                    case BlockFace::North:  return GetTileLayer(11, 3);
                    case BlockFace::South:  return GetTileLayer(11, 3);
                    case BlockFace::East:   return GetTileLayer(12, 3);
                    case BlockFace::West:   return GetTileLayer(12, 3);
                }
                break;
                
            case BlockType::Furnace:
                switch (face) {
                    case BlockFace::Top:    return GetTileLayer(14, 3);
                    case BlockFace::Bottom: return GetTileLayer(14, 3);
                    case BlockFace::North:  return GetTileLayer(12, 2);  // Front
                    case BlockFace::South:  return GetTileLayer(13, 2);  // Back/sides
                    case BlockFace::East:   return GetTileLayer(13, 2);
                    case BlockFace::West:   return GetTileLayer(13, 2);
                }
                break;
                
            case BlockType::Wood_Planks:
                return GetTileLayer(4, 0);

            default:
                return GetTileLayer(0, 0);  // Fallback
        }
        
        return GetTileLayer(0, 0);  // Default fallback
    }
}