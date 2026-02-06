#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/Mesh.hpp>

// Block types
enum class BlockType : uint8_t {
    Air,
    
    // Terrain
    Bedrock,
    Stone,
    Cobblestone,
    Dirt,
    Grass,
    Gravel,
    Sand,
    Sandstone,
    Clay,

    // Ores
    Coal_Ore,
    Iron_Ore,
    Gold_Ore,
    Diamond_Ore,

    // Wood & Plants
    Oak_Log,
    Oak_Leaves,
    Oak_Planks,
    Spruce_Log,
    Spruce_Leaves,
    Spruce_Planks,

    // Utility Blocks
    Crafting_Table,
    Furnace,

    // Special Blocks
    TNT,
    Obsidian,
    Mossy_Cobblestone,
    Bookshelf,
    Wool,
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
    // Get texture layer for a block face
    int GetBlockFaceLayer(BlockType blockType, BlockFace face);

    std::shared_ptr<ASCIIgL::Mesh> GetBlockMesh(BlockType blockType);
}
