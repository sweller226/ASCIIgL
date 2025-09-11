#include <ASCIICraft/world/Block.hpp>

#include <ASCIIgL/engine/Texture.hpp>

// Static texture atlas definition
Texture* Block::textureAtlas = nullptr;

// Get UV coordinates for the block's texture
glm::vec4 Block::GetTextureUV(int face) const {
    return BlockTextures::GetBlockFaceUV(type, static_cast<BlockFace>(face));
}

// Static texture atlas management
void Block::SetTextureAtlas(Texture* atlas) {
    textureAtlas = atlas;
}

Texture* Block::GetTextureAtlas() {
    return textureAtlas;
}

bool Block::HasTextureAtlas() {
    return textureAtlas != nullptr;
}

// Get UV coordinates for the block's texture
glm::vec4 Block::GetTextureUV(int face) const {
    return BlockTextures::GetBlockFaceUV(type, static_cast<BlockFace>(face));
}

namespace BlockTextures {
    
    // Convert tile coordinates to UV coordinates (16x16 atlas)
    glm::vec4 GetTileUV(int tileX, int tileY) {
        constexpr float ATLAS_SIZE = 16.0f;  // 16x16 tiles
        constexpr float TILE_SIZE = 1.0f / ATLAS_SIZE;
        
        float minU = tileX * TILE_SIZE;
        float minV = tileY * TILE_SIZE;
        float maxU = minU + TILE_SIZE;
        float maxV = minV + TILE_SIZE;
        
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
                    case BlockFace::Top:
                    case BlockFace::Bottom: return GetTileUV(5, 1);  // Wood top/bottom (rings)
                    default:                return GetTileUV(4, 1);  // Wood side (bark)
                }
                
            case BlockType::Leaves:
                return GetTileUV(4, 3);  // Leaves texture (all faces same)
                
            case BlockType::Sand:
                return GetTileUV(2, 1);  // Sand texture (all faces same)
                
            case BlockType::Gravel:
                return GetTileUV(3, 1);  // Gravel texture (all faces same)
                
            case BlockType::Coal_Ore:
                return GetTileUV(2, 2);  // Coal ore texture (all faces same)
                
            case BlockType::Iron_Ore:
                return GetTileUV(1, 2);  // Iron ore texture (all faces same)
                
            case BlockType::Gold_Ore:
                return GetTileUV(0, 2);  // Gold ore texture (all faces same)
                
            case BlockType::Diamond_Ore:
                return GetTileUV(2, 3);  // Diamond ore texture (all faces same)
                
            case BlockType::Bedrock:
                return GetTileUV(1, 1);  // Bedrock texture (all faces same)
                
            default:
                return GetTileUV(0, 0);  // Default to empty/air
        }
    }
}