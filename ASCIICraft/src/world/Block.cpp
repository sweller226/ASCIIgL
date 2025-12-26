#include <ASCIICraft/world/Block.hpp>

#include <ASCIIgL/engine/Texture.hpp>

// Static texture atlas definition
ASCIIgL::Texture* Block::textureAtlas = nullptr;

// Get UV coordinates for the block's texture
glm::vec4 Block::GetTextureUV(int face) const {
    return BlockTextures::GetBlockFaceUV(type, static_cast<BlockFace>(face));
}

// Static texture atlas management
void Block::SetTextureAtlas(ASCIIgL::Texture* atlas) {
    textureAtlas = atlas;
}

ASCIIgL::Texture* Block::GetTextureAtlas() {
    return textureAtlas;
}

bool Block::HasTextureAtlas() {
    return textureAtlas != nullptr;
}

namespace BlockTextures {
    
    // Convert tile coordinates to UV coordinates (dynamic atlas size)
    glm::vec4 GetTileUV(int tileX, int tileY) {
        if (!Block::HasTextureAtlas()) {
            return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);  // Fallback
        }
        
        ASCIIgL::Texture* atlas = Block::GetTextureAtlas();
        const int atlasWidth = atlas->GetWidth();
        const int atlasHeight = atlas->GetHeight();
        
        // Calculate tiles per row/column (assuming 16x16 pixel tiles)
        constexpr int TILE_PIXEL_SIZE = 16;
        const float tilesPerRow = static_cast<float>(atlasWidth) / TILE_PIXEL_SIZE;
        const float tilesPerCol = static_cast<float>(atlasHeight) / TILE_PIXEL_SIZE;
        
        const float tileWidth = 1.0f / tilesPerRow;
        const float tileHeight = 1.0f / tilesPerCol;
        
        float minU = tileX * tileWidth;
        float minV = tileY * tileHeight;
        float maxU = (tileX + 1) * tileWidth;
        float maxV = (tileY + 1) * tileHeight;
        
        return glm::vec4(minU, minV, maxU, maxV);
    }
    
    // Get texture coordinates for block faces based on Minecraft Beta 1.7.3 terrain.png
    glm::vec4 GetBlockFaceUV(BlockType blockType, BlockFace face) {
        switch (blockType) {
            case BlockType::Air:
                return GetTileUV(0, 0);  // Transparent/empty
                
            case BlockType::Stone:
                return GetTileUV(1, 0);  // Stone texture (all faces same)
                
            case BlockType::Dirt:
                return GetTileUV(2, 0);  // Dirt texture (all faces same)
                
            case BlockType::Grass:
                switch (face) {
                    case BlockFace::Top:    return GetTileUV(0, 0);  // Grass top
                    case BlockFace::Bottom: return GetTileUV(2, 0);  // Dirt bottom
                    default:                return GetTileUV(3, 0);  // Grass side
                }
                
            case BlockType::Wood:
                switch (face) {
                    case BlockFace::Top:    return GetTileUV(5, 1);
                    case BlockFace::Bottom: return GetTileUV(5, 1);  // Wood top/bottom (rings)
                    default:                return GetTileUV(4, 1);  // Wood side (bark)
                }
                
            case BlockType::Leaves:
                return GetTileUV(4, 3);  // Leaves texture (all faces same)
                
            case BlockType::Gravel:
                return GetTileUV(3, 1);  // Gravel texture (all faces same)
                
            case BlockType::Coal_Ore:
                return GetTileUV(2, 2);  // Coal ore texture (all faces same)
                
            case BlockType::Iron_Ore:
                return GetTileUV(1, 2);  // Iron ore texture (all faces same)

            case BlockType::Diamond_Ore:
                return GetTileUV(0, 2);  // Diamond ore texture (all faces same)
                
            case BlockType::Bedrock:
                return GetTileUV(1, 1);  // Bedrock texture (all faces same)

            case BlockType::Cobblestone:
                return GetTileUV(0, 1);  // Cobblestone texture (all faces same)

            case BlockType::Crafting_Table:
                switch (face) {
                    case BlockFace::Top:    return GetTileUV(11, 2);
                    case BlockFace::Bottom: return GetTileUV(4, 0);
                    case BlockFace::North:  return GetTileUV(11, 3);
                    case BlockFace::South:  return GetTileUV(11, 3);
                    case BlockFace::East:   return GetTileUV(12, 3);
                    case BlockFace::West:   return GetTileUV(12, 3);
                }
            case BlockType::Furnace:
                switch (face) {
                    case BlockFace::Top:    return GetTileUV(14, 3); 
                    case BlockFace::Bottom: return GetTileUV(14, 3); 
                    case BlockFace::North:  return GetTileUV(12, 2); 
                    case BlockFace::South:  return GetTileUV(13, 2); 
                    case BlockFace::East:   return GetTileUV(13, 2); 
                    case BlockFace::West:   return GetTileUV(13, 2); 
                }
            case BlockType::Wood_Planks:
                return GetTileUV(4, 0);

            default:
                return GetTileUV(0, 0);  // Default to empty/air
        }
    }
}