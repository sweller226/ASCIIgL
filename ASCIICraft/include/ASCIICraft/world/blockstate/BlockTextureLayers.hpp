#pragma once

// Logical indices into the terrain TextureArray used by ASCIICraft.
// These must match the order of blockTexturePaths in Game::LoadTextures.
enum class BlockTexLayer : int {
    Stone           = 0,  // stone.png
    Cobblestone     = 1,  // cobblestone.png
    Bedrock         = 2,  // bedrock.png
    Dirt            = 3,  // dirt.png
    OakLog          = 4,  // log_oak.png
    OakLogTop       = 5,  // log_oak_top.png
    OakLeaves       = 6,  // leaves_oak.png
    OakPlanks       = 7,  // planks_oak.png
    GrassTop        = 8,  // grass_top.png
    GrassSide       = 9,  // grass_side.png
    WaterStill      = 10, // water_still.png
    Sand            = 11, // sand.png
    Gravel          = 12, // gravel.png
    SpruceLog       = 13, // log_spruce.png
    SpruceLogTop    = 14, // log_spruce_top.png
    SpruceLeaves    = 15, // leaves_spruce.png
    SprucePlanks    = 16, // planks_spruce.png
    CraftingFront   = 17, // crafting_table_front.png
    CraftingSide    = 18, // crafting_table_side.png
    CraftingTop     = 19, // crafting_table_top.png
    FurnaceFrontOff = 20, // furnace_front_off.png
    FurnaceFrontOn  = 21, // furnace_front_on.png
    FurnaceSide     = 22, // furnace_side.png
    FurnaceTop      = 23, // furnace_top.png
    Brick           = 24, // brick.png
};

