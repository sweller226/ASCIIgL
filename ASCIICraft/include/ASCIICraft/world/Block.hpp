#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include <ASCIIgL/engine/TextureArray.hpp>

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

// Block face indices
enum class BlockFace : uint8_t {
    Top = 0,
    Bottom = 1,
    North = 2,   // Front (+Z)
    South = 3,   // Back (-Z)
    East = 4,    // Right (+X)
    West = 5     // Left (-X)
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
    
    // Get texture layer index for a block face (for Texture2DArray)
    int GetTextureLayer(BlockFace face) const;
    
    // Static texture array management
    static void SetTextureArray(ASCIIgL::TextureArray* texArray);
    static ASCIIgL::TextureArray* GetTextureArray();
    static bool HasTextureArray();

private:
    static ASCIIgL::TextureArray* blockTextureArray;
};

// Texture array helper functions
namespace BlockTextures {
    // Get texture layer index for a specific tile coordinate
    int GetTileLayer(int tileX, int tileY);
    
    // Get texture layer for a block face
    int GetBlockFaceLayer(BlockType blockType, BlockFace face);
}
