#include <ASCIICraft/game/Game.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>
#include <ASCIIgL/renderer/gpu/Material.hpp>

#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/engine/MipFilters.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/ecs/data/ItemIndex.hpp>

#include <ASCIICraft/ecs/factories/gui/InventoryGUIFactory.hpp>

// ecs components
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>

// shaders
#include <ASCIICraft/rendering/TerrainShaders.hpp>
#include <ASCIICraft/rendering/GUIShaders.hpp>

Game::Game()
    : gameState(GameState::Playing)
    , isRunning(false)
    , movementSystem(registry)
    , physicsSystem(registry)
    , renderSystem(registry)
    , cameraSystem(registry)
    , guiSystem(registry, eventBus)
    , blockUpdateSystem(registry, eventBus)
    , placingSystem(registry, eventBus)
    , miningSystem(registry, eventBus)
    , playerFactory(registry)
{
    ASCIIgL::Logger::Debug("Game constructor: systems created, registry bound.");
}

Game::~Game() {
    ASCIIgL::Logger::Debug("Game destructor called.");
    Shutdown();
}

bool Game::Initialize() {
    ASCIIgL::Logger::Info("Initializing ASCIICraft...");

    ASCIIgL::Logger::Debug("Setting up palette and screen...");

    float lowLight = ASCIIgL::PaletteUtil::sRGB255_Luminance(glm::ivec3(18, 18, 18));
    float highLight = ASCIIgL::PaletteUtil::sRGB255_Luminance(glm::ivec3(210, 210, 210));

    // gray
    ASCIIgL::Palette grayPalette(
        lowLight,
        highLight,
        glm::ivec3(205, 205, 205)
    );

    // silver blue
    ASCIIgL::Palette silverBluePalette(
        lowLight,
        highLight,
        glm::ivec3(190, 205, 230)
    );

    // frost cyan
    ASCIIgL::Palette frostCyanPalette(
        lowLight,
        highLight,
        glm::ivec3(200, 230, 245)
    );

    // warm beige
    ASCIIgL::Palette warmBeigePalette(
        lowLight,
        highLight,
        glm::ivec3(220, 205, 180)
    );

    // slate
    ASCIIgL::Palette slatePalette(
        lowLight,
        highLight,
        glm::ivec3(180, 200, 230)
    );

    // choose your active palette
    ASCIIgL::Palette gamePalette = silverBluePalette;

    ASCIIgL::Logger::Debug("Initializing screen...");
    if (ASCIIgL::Screen::GetInst().Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, L"ASCIICraft", FONT_SIZE, gamePalette) != 0) {
        ASCIIgL::Logger::Error("Failed to initialize screen");
        return false;
    }

    SCREEN_WIDTH = ASCIIgL::Screen::GetInst().GetWidth();
    SCREEN_HEIGHT = ASCIIgL::Screen::GetInst().GetHeight();
    ASCIIgL::Logger::Debug("Screen initialized: " + std::to_string(SCREEN_WIDTH) + "x" + std::to_string(SCREEN_HEIGHT));

    ASCIIgL::FPSClock::GetInst().Initialize(static_cast<unsigned int>(TARGET_FPS), 1.0f);
    ASCIIgL::Logger::Debug("FPSClock initialized with target FPS: " + std::to_string(TARGET_FPS));

    ASCIIgL::Renderer::GetInst().SetBackgroundCol(gamePalette.GetRGB(0));
    ASCIIgL::Renderer::GetInst().SetWireframe(false);
    ASCIIgL::Renderer::GetInst().SetBackfaceCulling(true);
    ASCIIgL::Renderer::GetInst().SetCCW(true);
    ASCIIgL::Renderer::GetInst().SetDiagnosticsEnabled(true);
    
    // Using a monochromatic gradient palette, so use Monochrome optimization
    ASCIIgL::Renderer::GetInst().SetPaletteMode(ASCIIgL::PaletteMode::Monochrome);

    ASCIIgL::Logger::Debug("Initializing renderer...");
    ASCIIgL::Renderer::GetInst().Initialize(true, 4, false);

    ASCIIgL::Logger::Debug("Initializing ECS context...");
    InitializeContext();

    ASCIIgL::Logger::Debug("Initializing item definitions...");
    InitializeItemDefinitions();

    ASCIIgL::Logger::Debug("Initializing ECS systems...");
    InitializeSystems();

    ASCIIgL::Logger::Debug("Loading resources...");
    if (!LoadResources()) {
        ASCIIgL::Logger::Error("Failed to load resources");
        return false;
    }

    ASCIIgL::InputManager::GetInst().Initialize();
    ASCIIgL::Logger::Debug("InputManager initialized.");

    gameState = GameState::Playing;
    isRunning = true;

    ASCIIgL::Logger::Info("ASCIICraft initialized successfully!");
    return true;
}

void Game::Run() {
    if (!Initialize()) {
        ASCIIgL::Logger::Error("Failed to initialize game");
        return;
    }

    ASCIIgL::Logger::Info("Starting game loop...");

    ASCIIgL::Profiler::GetInst().SetEnabled(true);

    int frameCounter = 0;
    while (isRunning) {
        ASCIIgL::Profiler::GetInst().BeginFrame();
        ASCIIgL::FPSClock::GetInst().StartFPSClock();

        {
            ASCIIgL::PROFILE_SCOPE("Update");
            Update();
        }

        {
            ASCIIgL::PROFILE_SCOPE("RenderGame");
            Render();
        }

        eventBus.endFrame();

        ASCIIgL::FPSClock::GetInst().EndFPSClock();
        ASCIIgL::Profiler::GetInst().EndFrame();
        frameCounter++;

        if (frameCounter % 60 == 0) {
            ASCIIgL::Logger::Debug("Frame milestone reached: 60 frames processed.");
            ASCIIgL::Logger::Info("FPS: " + std::to_string(ASCIIgL::FPSClock::GetInst().GetFPS()));
            ASCIIgL::Profiler::GetInst().LogReport();
            ASCIIgL::Profiler::GetInst().Reset();
            frameCounter = 0;
        }
    }

    Shutdown();
}

void Game::Update() {
    ASCIIgL::Logger::Debug("Game::Update - state = " +
        std::to_string(static_cast<int>(gameState)));

    ASCIIgL::InputManager::GetInst().Update();

    if (ASCIIgL::InputManager::GetInst().IsActionPressed("quit")) {
        ASCIIgL::Logger::Info("Quit action detected. Exiting game...");
        gameState = GameState::Exiting;
    }

    switch (gameState) {
        case GameState::Playing: {

            ASCIIgL::Logger::Debug("Update: Retrieving world pointer");
            World* world = GetWorldPtr(registry);

            if (!world) {
                ASCIIgL::Logger::Error("Update aborted: World pointer is null");
                return;
            }

            // GUI system — always runs (handles 'E' toggle etc.)
            guiSystem.Update();

            // Skip game systems when a blocking panel is open
            if (!guiSystem.IsBlockingInput()) {
                // Block update systems
                ASCIIgL::Logger::Debug("Update: Running mining system");
                miningSystem.Update();
                
                ASCIIgL::Logger::Debug("Update: Running placement system");
                placingSystem.Update();

                ASCIIgL::Logger::Debug("Update: Running block update system");
                blockUpdateSystem.Update();

                ASCIIgL::Logger::Debug("Update: Running world update");
                world->Update();

                // Movement
                ASCIIgL::Logger::Debug("Update: Running movement system");
                movementSystem.Update();

                // Camera
                ASCIIgL::Logger::Debug("Update: Running camera system");
                cameraSystem.Update();

                // Physics
                ASCIIgL::Logger::Debug("Update: Running physics system");
                physicsSystem.Update();
            }


            break;
        }

        case GameState::Exiting:
            ASCIIgL::Logger::Info("GameState::Exiting triggered. Stopping game loop.");
            isRunning = false;
            break;
    }
}

void Game::Render() {
    ASCIIgL::Logger::Debug("Game::Render called.");

    {
        ASCIIgL::PROFILE_SCOPE("Clear Px Buff/Begin Frame");
        ASCIIgL::Screen::GetInst().ClearPixelBuffer();
        ASCIIgL::Renderer::GetInst().BeginColBuffFrame();
    }

    switch (gameState) {
        case GameState::Playing:
            {
                ASCIIgL::PROFILE_SCOPE("Render.RenderPlaying");
                RenderPlaying();
            }
            break;
    }

    {
        ASCIIgL::PROFILE_SCOPE("Render.EndColBuffFrame");
        ASCIIgL::Renderer::GetInst().EndColBuffFrame();
    }

    {
        ASCIIgL::PROFILE_SCOPE("Render.PixelBufferDraws");
        ASCIIgL::Renderer::GetInst().DrawScreenBorderPxBuff(0xF);
    }

    {
        ASCIIgL::PROFILE_SCOPE("Render.PixelBufferOutput");
        ASCIIgL::Screen::GetInst().OutputBuffer();
    }
}

void Game::Shutdown() {
    ASCIIgL::Logger::Info("Shutting down ASCIICraft...");

    Block::SetTextureArray(nullptr);
    // blockShaderProgram is now managed by MaterialLibrary/Material
    
    // Clear libraries if we want to release all resources on shutdown
    ASCIIgL::MaterialLibrary::GetInst().Clear();
    ASCIIgL::TextureLibrary::GetInst().ClearTextureArrays();

    ASCIIgL::Logger::Info("ASCIICraft shutdown complete");
}

bool Game::LoadResources() {
    ASCIIgL::Logger::Info("Loading game resources...");

    // Load block textures via TextureLibrary which takes ownership
    auto blockTextureArray = ASCIIgL::TextureLibrary::GetInst().LoadTextureArray("res/textures/terrain.png", 16, "terrainTextureArray");

    if (!blockTextureArray || !blockTextureArray->IsValid()) {
        ASCIIgL::Logger::Error("Failed to load block texture array");
        return false;
    }

    // Set static pointer for block system
    Block::SetTextureArray(blockTextureArray.get());
    
    // Create gradient mapping shader program
    auto terrainVS = ASCIIgL::Shader::CreateFromSource(
        TerrainShaders::GetTerrainVSSource(),
        ASCIIgL::ShaderType::Vertex
    );
    
    auto terrainPS = ASCIIgL::Shader::CreateFromSource(
        TerrainShaders::GetTerrainPSSource(),
        ASCIIgL::ShaderType::Pixel
    );
    
    if (!terrainVS || !terrainVS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile gradient map vertex shader: " + terrainVS->GetCompileError());
        return false;
    }
    
    if (!terrainPS || !terrainPS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile gradient map pixel shader: " + terrainPS->GetCompileError());
        return false;
    }
    
    // Create shader program with gradient map uniform layout
    auto blockShaderProgram = ASCIIgL::ShaderProgram::Create(
        std::move(terrainVS),
        std::move(terrainPS),
        ASCIIgL::VertFormats::PosUVLayer(),
        TerrainShaders::GetTerrainPSUniformLayout()
    );
    
    if (!blockShaderProgram || !blockShaderProgram->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create gradient map shader program");
        return false;
    }

    std::shared_ptr<ASCIIgL::ShaderProgram> sharedProgram;

    // Create material and register it
    sharedProgram = std::move(blockShaderProgram);
    auto blockMaterial = ASCIIgL::Material::Create(sharedProgram);
    
    if (!blockMaterial) {
        ASCIIgL::Logger::Error("Failed to create block material");
        return false;
    }
    
    // Set texture array (weak reference inside Material, owned by TextureLibrary)
    blockMaterial->SetTextureArray(0, blockTextureArray.get());
    
    // Set gradient colors (dark to bright, normalized 0-1 range)
    // These correspond to palette colors: dark end = black, bright end = white
    auto& palette = ASCIIgL::Screen::GetInst().GetPalette();
    blockMaterial->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(0), 1.0f));  // Black
    blockMaterial->SetFloat4("gradientEnd", glm::vec4(palette.GetRGBNormalized(15), 1.0f));  // Bright (matches palette)

    // Register material
    ASCIIgL::MaterialLibrary::GetInst().Register("blockMaterial", std::move(blockMaterial));

    auto inventoryTextureArray = ASCIIgL::TextureLibrary::GetInst().LoadTextureArray("res/textures/gui/inventory.png", 16, "inventoryTextureArray");
    if (!inventoryTextureArray || !inventoryTextureArray->IsValid()) {
        ASCIIgL::Logger::Error("Failed to load inventory texture array");
        return false;
    }

    auto guiVS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetGUIVSSource(),
        ASCIIgL::ShaderType::Vertex
    );

    auto guiPS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetGUIPSSource(),
        ASCIIgL::ShaderType::Pixel
    );

    if (!guiVS || !guiVS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile GUI vertex shader: " + guiVS->GetCompileError());
        return false;
    }

    if (!guiPS || !guiPS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile GUI pixel shader: " + guiPS->GetCompileError());
        return false;
    }

    auto guiShaderProgram = ASCIIgL::ShaderProgram::Create(
        std::move(guiVS),
        std::move(guiPS),
        ASCIIgL::VertFormats::PosUV(),
        GUIShaders::GetGUIPSUniformLayout()
    );

    if (!guiShaderProgram || !guiShaderProgram->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create GUI shader program");
        return false;
    }

    sharedProgram = std::move(guiShaderProgram);
    auto guiMaterial = ASCIIgL::Material::Create(sharedProgram);
    
    if (!guiMaterial) {
        ASCIIgL::Logger::Error("Failed to create GUI material");
        return false;
    }
    
    // Register material
    ASCIIgL::MaterialLibrary::GetInst().Register("guiMaterial", std::move(guiMaterial));

    ASCIIgL::Logger::Info("Resources loaded successfully");
    return true;
}

void Game::RenderPlaying() {
    ASCIIgL::Logger::Debug("RenderPlaying: binding shader and texture array");
    
    ASCIIgL::Logger::Debug("RenderPlaying: rendering world");
    GetWorldPtr(registry)->Render();
    
    ASCIIgL::Logger::Debug("RenderPlaying: rendering systems");
    renderSystem.Render();
}

void Game::InitializeContext() {
    ASCIIgL::Logger::Debug("Creating world...");

    std::unique_ptr<World> world = std::make_unique<World>(registry, WorldCoord(0, 90, 0), 8);
    registry.ctx().emplace<std::unique_ptr<World>>(std::move(world));

    ASCIIgL::Logger::Debug("World created and stored in registry context.");

    // Create player entity using factory member
    playerFactory.createPlayerEnt(GetWorldPtr(registry)->GetSpawnPoint().ToVec3(), GameMode::Survival);
    ASCIIgL::Logger::Debug("Player entity created");
}

void Game::InitializeSystems() {
    ASCIIgL::Logger::Debug("Initializing systems...");

    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player != entt::null) {
        renderSystem.SetActive3DCamera(registry.try_get<ecs::components::PlayerCamera>(player));
    }

    // GUI system setup
    guiSystem.SetScreenSize(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT));
    guiSystem.CreateCursor();

    // Create inventory panel bound to player
    if (player != entt::null) {
        ecs::factories::gui::InventoryGUIFactory invFactory;
        invFactory.Create(registry, player);
    }

    ASCIIgL::Logger::Debug("Systems initialized.");
}

void Game::InitializeItemDefinitions() {
    auto& itemIndex = registry.ctx().emplace<ecs::data::ItemIndex>();

    // === Terrain === (IDs auto-assigned: 1, 2, 3, ...)
    itemIndex.RegisterBlockItem(registry, "stone",       "Stone",       BlockTextures::GetBlockMesh(BlockType::Stone));
    itemIndex.RegisterBlockItem(registry, "cobblestone", "Cobblestone", BlockTextures::GetBlockMesh(BlockType::Cobblestone));
    itemIndex.RegisterBlockItem(registry, "dirt",        "Dirt",        BlockTextures::GetBlockMesh(BlockType::Dirt));
    itemIndex.RegisterBlockItem(registry, "grass",       "Grass Block", BlockTextures::GetBlockMesh(BlockType::Grass));
    itemIndex.RegisterBlockItem(registry, "gravel",      "Gravel",      BlockTextures::GetBlockMesh(BlockType::Gravel));
    itemIndex.RegisterBlockItem(registry, "sand",        "Sand",        BlockTextures::GetBlockMesh(BlockType::Sand));
    itemIndex.RegisterBlockItem(registry, "sandstone",   "Sandstone",   BlockTextures::GetBlockMesh(BlockType::Sandstone));
    itemIndex.RegisterBlockItem(registry, "clay",        "Clay",        BlockTextures::GetBlockMesh(BlockType::Clay));
    itemIndex.RegisterBlockItem(registry, "bedrock",     "Bedrock",     BlockTextures::GetBlockMesh(BlockType::Bedrock));

    // === Ores ===
    itemIndex.RegisterBlockItem(registry, "coal_ore",    "Coal Ore",    BlockTextures::GetBlockMesh(BlockType::Coal_Ore));
    itemIndex.RegisterBlockItem(registry, "iron_ore",    "Iron Ore",    BlockTextures::GetBlockMesh(BlockType::Iron_Ore));
    itemIndex.RegisterBlockItem(registry, "gold_ore",    "Gold Ore",    BlockTextures::GetBlockMesh(BlockType::Gold_Ore));
    itemIndex.RegisterBlockItem(registry, "diamond_ore", "Diamond Ore", BlockTextures::GetBlockMesh(BlockType::Diamond_Ore));

    // === Wood & Plants ===
    itemIndex.RegisterBlockItem(registry, "oak_log",       "Oak Log",       BlockTextures::GetBlockMesh(BlockType::Oak_Log));
    itemIndex.RegisterBlockItem(registry, "oak_leaves",    "Oak Leaves",    BlockTextures::GetBlockMesh(BlockType::Oak_Leaves));
    itemIndex.RegisterBlockItem(registry, "oak_planks",    "Oak Planks",    BlockTextures::GetBlockMesh(BlockType::Oak_Planks));
    itemIndex.RegisterBlockItem(registry, "spruce_log",    "Spruce Log",    BlockTextures::GetBlockMesh(BlockType::Spruce_Log));
    itemIndex.RegisterBlockItem(registry, "spruce_leaves", "Spruce Leaves", BlockTextures::GetBlockMesh(BlockType::Spruce_Leaves));
    itemIndex.RegisterBlockItem(registry, "spruce_planks", "Spruce Planks", BlockTextures::GetBlockMesh(BlockType::Spruce_Planks));

    // === Utility Blocks ===
    itemIndex.RegisterBlockItem(registry, "crafting_table", "Crafting Table", BlockTextures::GetBlockMesh(BlockType::Crafting_Table));
    itemIndex.RegisterBlockItem(registry, "furnace",        "Furnace",        BlockTextures::GetBlockMesh(BlockType::Furnace));

    // === Special Blocks ===
    itemIndex.RegisterBlockItem(registry, "tnt",                "TNT",                BlockTextures::GetBlockMesh(BlockType::TNT));
    itemIndex.RegisterBlockItem(registry, "obsidian",           "Obsidian",           BlockTextures::GetBlockMesh(BlockType::Obsidian));
    itemIndex.RegisterBlockItem(registry, "mossy_cobblestone",  "Mossy Cobblestone",  BlockTextures::GetBlockMesh(BlockType::Mossy_Cobblestone));
    itemIndex.RegisterBlockItem(registry, "bookshelf",          "Bookshelf",          BlockTextures::GetBlockMesh(BlockType::Bookshelf));
    itemIndex.RegisterBlockItem(registry, "wool",               "Wool",               BlockTextures::GetBlockMesh(BlockType::Wool));

    // === Resources / Materials ===
    using IX = ecs::data::ItemIndex;
    itemIndex.RegisterResourceItem(registry, 263, "coal",       "Coal",       IX::GetQuadItemMesh(7, 10));
    itemIndex.RegisterResourceItem(registry, 264, "diamond",    "Diamond",    IX::GetQuadItemMesh(8, 10));
    itemIndex.RegisterResourceItem(registry, 265, "iron_ingot", "Iron Ingot", IX::GetQuadItemMesh(9, 10));
    itemIndex.RegisterResourceItem(registry, 266, "gold_ingot", "Gold Ingot", IX::GetQuadItemMesh(10, 10));
    itemIndex.RegisterResourceItem(registry, 280, "stick",      "Stick",      IX::GetQuadItemMesh(5, 9));

    // === Swords ===
    itemIndex.RegisterToolItem(registry, 268, "wooden_sword",  "Wooden Sword",  IX::GetQuadItemMesh(0, 4), {2.0f, 1, 60},   {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 272, "stone_sword",   "Stone Sword",   IX::GetQuadItemMesh(1, 4), {4.0f, 2, 132},  {5.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 267, "iron_sword",    "Iron Sword",    IX::GetQuadItemMesh(2, 4), {6.0f, 3, 251},  {6.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 276, "diamond_sword", "Diamond Sword", IX::GetQuadItemMesh(3, 4), {8.0f, 4, 1562}, {7.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 283, "gold_sword",    "Golden Sword",  IX::GetQuadItemMesh(4, 4), {12.0f, 1, 33},  {4.0f, 1.6f});

    // === Shovels ===
    itemIndex.RegisterToolItem(registry, 269, "wooden_shovel",  "Wooden Shovel",  IX::GetQuadItemMesh(0, 5), {2.0f, 1, 60},   {1.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 273, "stone_shovel",   "Stone Shovel",   IX::GetQuadItemMesh(1, 5), {4.0f, 2, 132},  {2.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 256, "iron_shovel",    "Iron Shovel",    IX::GetQuadItemMesh(2, 5), {6.0f, 3, 251},  {3.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 277, "diamond_shovel", "Diamond Shovel", IX::GetQuadItemMesh(3, 5), {8.0f, 4, 1562}, {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 284, "gold_shovel",    "Golden Shovel",  IX::GetQuadItemMesh(4, 5), {12.0f, 1, 33},  {1.0f, 1.6f});

    // === Pickaxes ===
    itemIndex.RegisterToolItem(registry, 270, "wooden_pickaxe",  "Wooden Pickaxe",  IX::GetQuadItemMesh(0, 6), {2.0f, 1, 60},   {2.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 274, "stone_pickaxe",   "Stone Pickaxe",   IX::GetQuadItemMesh(1, 6), {4.0f, 2, 132},  {3.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 257, "iron_pickaxe",    "Iron Pickaxe",    IX::GetQuadItemMesh(2, 6), {6.0f, 3, 251},  {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 278, "diamond_pickaxe", "Diamond Pickaxe", IX::GetQuadItemMesh(3, 6), {8.0f, 4, 1562}, {5.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 285, "gold_pickaxe",    "Golden Pickaxe",  IX::GetQuadItemMesh(4, 6), {12.0f, 1, 33},  {2.0f, 1.6f});

    // === Axes ===
    itemIndex.RegisterToolItem(registry, 271, "wooden_axe",  "Wooden Axe",  IX::GetQuadItemMesh(0, 7), {2.0f, 1, 60},   {3.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 275, "stone_axe",   "Stone Axe",   IX::GetQuadItemMesh(1, 7), {4.0f, 2, 132},  {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 258, "iron_axe",    "Iron Axe",    IX::GetQuadItemMesh(2, 7), {6.0f, 3, 251},  {5.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 279, "diamond_axe", "Diamond Axe", IX::GetQuadItemMesh(3, 7), {8.0f, 4, 1562}, {6.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 286, "gold_axe",    "Golden Axe",  IX::GetQuadItemMesh(4, 7), {12.0f, 1, 33},  {3.0f, 1.6f});
}
