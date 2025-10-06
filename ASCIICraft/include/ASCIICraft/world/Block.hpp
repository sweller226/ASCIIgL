#pragma once

#include <cstdint>
#include <glm/glm.hpp>

// Forward declaration
class Texture;

// Block types
enum class BlockType : uint8_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Wood = 4,
    Leaves = 5,
    Gravel = 8,
    Coal_Ore = 9,
    Iron_Ore = 10,
    Diamond_Ore = 12,
    Cobblestone = 13,
    Crafting_Table = 14,
    Wood_Planks = 15,
    Furnace = 16,
    Bedrock = 17,
};

// Individual block data
struct Block {
    BlockType type;
    uint8_t metadata;  // For block variations (damage, orientation, etc.)
    
    Block() : type(BlockType::Air), metadata(0) {}
    Block(BlockType t, uint8_t meta = 0) : type(t), metadata(meta) {}
    
    bool IsSolid() const {
        return type != BlockType::Air;
    }
    
    // Get UV coordinates for texture atlas (Minecraft Beta 1.7.3)
    glm::vec4 GetTextureUV(int face = 0) const;
    
    // Static texture atlas management
    static void SetTextureAtlas(Texture* atlas);
    static Texture* GetTextureAtlas();
    static bool HasTextureAtlas();

private:
    static Texture* textureAtlas;  // Global texture atlas for all blocks
};

// Block face indices for GetTextureUV
enum class BlockFace {
    Top = 0,
    Bottom = 1,
    North = 2,   // Front
    South = 3,   // Back  
    East = 4,    // Right
    West = 5     // Left
};

// Texture atlas helper functions
namespace BlockTextures {
    // Get UV coordinates for a specific tile in the 16x16 atlas
    glm::vec4 GetTileUV(int tileX, int tileY);
    
    // Get texture coordinates for block faces (returns vec4: minU, minV, maxU, maxV)
    glm::vec4 GetBlockFaceUV(BlockType blockType, BlockFace face);
}
