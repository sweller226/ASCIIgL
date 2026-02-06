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
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

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
    ASCIIgL::Palette gamePalette = slatePalette;

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
    ASCIIgL::Logger::Debug("Initializing render system camera...");

    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player != entt::null) {
        renderSystem.SetActive3DCamera(registry.try_get<ecs::components::PlayerCamera>(player));
    }

    ASCIIgL::Logger::Debug("Systems initialized.");
}

void Game::InitializeItemDefinitions() {
    auto& registry = ecs::data::ItemRegistry::Instance();

    using ir = ecs::data::ItemRegistry;
    using id = ecs::data::ItemDefinition;

    // === Terrain ===
    {
        ir::ItemDef stone = ir::MakeItemDef(
            1, "stone", "Stone", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Stone), false
        );
        stone.set(PlaceableProperty{ 1 });
        registry.RegisterItem(stone);

        ir::ItemDef cobblestone = ir::MakeItemDef(
            2, "cobblestone", "Cobblestone", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Cobblestone), false
        );
        cobblestone.set(PlaceableProperty{ 2 });
        registry.RegisterItem(cobblestone);

        ir::ItemDef dirt = ir::MakeItemDef(
            3, "dirt", "Dirt", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Dirt), false
        );
        dirt.set(PlaceableProperty{ 3 });
        registry.RegisterItem(dirt);

        ir::ItemDef grass = ir::MakeItemDef(
            4, "grass", "Grass Block", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Grass), false
        );
        grass.set(PlaceableProperty{ 4 });
        registry.RegisterItem(grass);

        ir::ItemDef gravel = ir::MakeItemDef(
            5, "gravel", "Gravel", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Gravel), false
        );
        gravel.set(PlaceableProperty{ 5 });
        registry.RegisterItem(gravel);

        ir::ItemDef sand = ir::MakeItemDef(
            6, "sand", "Sand", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Sand), false
        );
        sand.set(PlaceableProperty{ 6 });
        registry.RegisterItem(sand);

        ir::ItemDef sandstone = ir::MakeItemDef(
            7, "sandstone", "Sandstone", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Sandstone), false
        );
        sandstone.set(PlaceableProperty{ 7 });
        registry.RegisterItem(sandstone);

        ir::ItemDef clay = ir::MakeItemDef(
            8, "clay", "Clay", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Clay), false
        );
        clay.set(PlaceableProperty{ 8 });
        registry.RegisterItem(clay);

        ir::ItemDef bedrock = ir::MakeItemDef(
            9, "bedrock", "Bedrock", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Bedrock), false
        );
        bedrock.set(PlaceableProperty{ 9 });
        registry.RegisterItem(bedrock);
    }

    // === Ores ===
    {
        ir::ItemDef coalOre = ir::MakeItemDef(
            10, "coal_ore", "Coal Ore", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Coal_Ore), false
        );
        coalOre.set(PlaceableProperty{ 10 });
        registry.RegisterItem(coalOre);

        ir::ItemDef ironOre = ir::MakeItemDef(
            11, "iron_ore", "Iron Ore", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Iron_Ore), false
        );
        ironOre.set(PlaceableProperty{ 11 });
        registry.RegisterItem(ironOre);

        ir::ItemDef goldOre = ir::MakeItemDef(
            12, "gold_ore", "Gold Ore", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Gold_Ore), false
        );
        goldOre.set(PlaceableProperty{ 12 });
        registry.RegisterItem(goldOre);

        ir::ItemDef diamondOre = ir::MakeItemDef(
            13, "diamond_ore", "Diamond Ore", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Diamond_Ore), false
        );
        diamondOre.set(PlaceableProperty{ 13 });
        registry.RegisterItem(diamondOre);
    }

    // === Wood & Plants ===
    {
        ir::ItemDef oakLog = ir::MakeItemDef(
            14, "oak_log", "Oak Log", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Oak_Log), false
        );
        oakLog.set(PlaceableProperty{ 14 });
        registry.RegisterItem(oakLog);

        ir::ItemDef oakLeaves = ir::MakeItemDef(
            15, "oak_leaves", "Oak Leaves", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Oak_Leaves), false
        );
        oakLeaves.set(PlaceableProperty{ 15 });
        registry.RegisterItem(oakLeaves);

        ir::ItemDef oakPlanks = ir::MakeItemDef(
            16, "oak_planks", "Oak Planks", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Oak_Planks), false
        );
        oakPlanks.set(PlaceableProperty{ 16 });
        registry.RegisterItem(oakPlanks);

        ir::ItemDef spruceLog = ir::MakeItemDef(
            17, "spruce_log", "Spruce Log", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Spruce_Log), false
        );
        spruceLog.set(PlaceableProperty{ 17 });
        registry.RegisterItem(spruceLog);

        ir::ItemDef spruceLeaves = ir::MakeItemDef(
            18, "spruce_leaves", "Spruce Leaves", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Spruce_Leaves), false
        );
        spruceLeaves.set(PlaceableProperty{ 18 });
        registry.RegisterItem(spruceLeaves);

        ir::ItemDef sprucePlanks = ir::MakeItemDef(
            19, "spruce_planks", "Spruce Planks", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Spruce_Planks), false
        );
        sprucePlanks.set(PlaceableProperty{ 19 });
        registry.RegisterItem(sprucePlanks);
    }

    // === Utility Blocks ===
    {
        ir::ItemDef craftingTable = ir::MakeItemDef(
            20, "crafting_table", "Crafting Table", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Crafting_Table), false
        );
        craftingTable.set(PlaceableProperty{ 20 });
        registry.RegisterItem(craftingTable);

        ir::ItemDef furnace = ir::MakeItemDef(
            21, "furnace", "Furnace", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Furnace), false
        );
        furnace.set(PlaceableProperty{ 21 });
        registry.RegisterItem(furnace);
    }

    // === Special Blocks ===
    {
        ir::ItemDef tnt = ir::MakeItemDef(
            22, "tnt", "TNT", 64, true,
            BlockTextures::GetBlockMesh(BlockType::TNT), false
        );
        tnt.set(PlaceableProperty{ 22 });
        registry.RegisterItem(tnt);

        ir::ItemDef obsidian = ir::MakeItemDef(
            23, "obsidian", "Obsidian", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Obsidian), false
        );
        obsidian.set(PlaceableProperty{ 23 });
        registry.RegisterItem(obsidian);

        ir::ItemDef mossyCobble = ir::MakeItemDef(
            24, "mossy_cobblestone", "Mossy Cobblestone", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Mossy_Cobblestone), false
        );
        mossyCobble.set(PlaceableProperty{ 24 });
        registry.RegisterItem(mossyCobble);

        ir::ItemDef bookshelf = ir::MakeItemDef(
            25, "bookshelf", "Bookshelf", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Bookshelf), false
        );
        bookshelf.set(PlaceableProperty{ 25 });
        registry.RegisterItem(bookshelf);

        ir::ItemDef wool = ir::MakeItemDef(
            26, "wool", "Wool", 64, true,
            BlockTextures::GetBlockMesh(BlockType::Wool), false
        );
        wool.set(PlaceableProperty{ 26 });
        registry.RegisterItem(wool);
    }

    // === Resources / Materials ===
    {
        ir::ItemDef coal = ir::MakeItemDef(
            263, "coal", "Coal", 64, true,
            ir::GetQuadItemMesh(7, 10), true
        );
        registry.RegisterItem(coal);

        ir::ItemDef diamond = ir::MakeItemDef(
            264, "diamond", "Diamond", 64, true,
            ir::GetQuadItemMesh(8, 10), true
        );
        registry.RegisterItem(diamond);

        ir::ItemDef ironIngot = ir::MakeItemDef(
            265, "iron_ingot", "Iron Ingot", 64, true,
            ir::GetQuadItemMesh(9, 10), true
        );
        registry.RegisterItem(ironIngot);

        ir::ItemDef goldIngot = ir::MakeItemDef(
            266, "gold_ingot", "Gold Ingot", 64, true,
            ir::GetQuadItemMesh(10, 10), true
        );
        registry.RegisterItem(goldIngot);

        ir::ItemDef stick = ir::MakeItemDef(
            280, "stick", "Stick", 64, true,
            ir::GetQuadItemMesh(5, 9), true
        );
        registry.RegisterItem(stick);
    }

    // === Tools ===

    // Swords
    {
        ir::ItemDef woodenSword = ir::MakeItemDef(
            268, "wooden_sword", "Wooden Sword", 1, false,
            ir::GetQuadItemMesh(0, 4), true
        );
        woodenSword.set(WeaponProperty{ 4.0f, 1.6f });
        woodenSword.set(ToolProperty{ 2.0f, 1, 60 });
        registry.RegisterItem(woodenSword);

        ir::ItemDef stoneSword = ir::MakeItemDef(
            272, "stone_sword", "Stone Sword", 1, false,
            ir::GetQuadItemMesh(1, 4), true
        );
        stoneSword.set(WeaponProperty{ 5.0f, 1.6f });
        stoneSword.set(ToolProperty{ 4.0f, 2, 132 });
        registry.RegisterItem(stoneSword);

        ir::ItemDef ironSword = ir::MakeItemDef(
            267, "iron_sword", "Iron Sword", 1, false,
            ir::GetQuadItemMesh(2, 4), true
        );
        ironSword.set(WeaponProperty{ 6.0f, 1.6f });
        ironSword.set(ToolProperty{ 6.0f, 3, 251 });
        registry.RegisterItem(ironSword);

        ir::ItemDef diamondSword = ir::MakeItemDef(
            276, "diamond_sword", "Diamond Sword", 1, false,
            ir::GetQuadItemMesh(3, 4), true
        );
        diamondSword.set(WeaponProperty{ 7.0f, 1.6f });
        diamondSword.set(ToolProperty{ 8.0f, 4, 1562 });
        registry.RegisterItem(diamondSword);

        ir::ItemDef goldSword = ir::MakeItemDef(
            283, "gold_sword", "Golden Sword", 1, false,
            ir::GetQuadItemMesh(4, 4), true
        );
        goldSword.set(WeaponProperty{ 4.0f, 1.6f });
        goldSword.set(ToolProperty{ 12.0f, 1, 33 });
        registry.RegisterItem(goldSword);
    }

    // Shovels
    {
        ir::ItemDef woodenShovel = ir::MakeItemDef(
            269, "wooden_shovel", "Wooden Shovel", 1, false,
            ir::GetQuadItemMesh(0, 5), true
        );
        woodenShovel.set(ToolProperty{ 2.0f, 1, 60 });
        woodenShovel.set(WeaponProperty{ 1.0f, 1.6f });
        registry.RegisterItem(woodenShovel);

        ir::ItemDef stoneShovel = ir::MakeItemDef(
            273, "stone_shovel", "Stone Shovel", 1, false,
            ir::GetQuadItemMesh(1, 5), true
        );
        stoneShovel.set(ToolProperty{ 4.0f, 2, 132 });
        stoneShovel.set(WeaponProperty{ 2.0f, 1.6f });
        registry.RegisterItem(stoneShovel);

        ir::ItemDef ironShovel = ir::MakeItemDef(
            256, "iron_shovel", "Iron Shovel", 1, false,
            ir::GetQuadItemMesh(2, 5), true
        );
        ironShovel.set(ToolProperty{ 6.0f, 3, 251 });
        ironShovel.set(WeaponProperty{ 3.0f, 1.6f });
        registry.RegisterItem(ironShovel);

        ir::ItemDef diamondShovel = ir::MakeItemDef(
            277, "diamond_shovel", "Diamond Shovel", 1, false,
            ir::GetQuadItemMesh(3, 5), true
        );
        diamondShovel.set(ToolProperty{ 8.0f, 4, 1562 });
        diamondShovel.set(WeaponProperty{ 4.0f, 1.6f });
        registry.RegisterItem(diamondShovel);

        ir::ItemDef goldShovel = ir::MakeItemDef(
            284, "gold_shovel", "Golden Shovel", 1, false,
            ir::GetQuadItemMesh(4, 5), true
        );
        goldShovel.set(ToolProperty{ 12.0f, 1, 33 });
        goldShovel.set(WeaponProperty{ 1.0f, 1.6f });
        registry.RegisterItem(goldShovel);
    }

    // Pickaxes
    {
        ir::ItemDef woodenAxe = ir::MakeItemDef(
            271, "wooden_axe", "Wooden Axe", 1, false,
            ir::GetQuadItemMesh(0, 7), true
        );
        woodenAxe.set(ToolProperty{ 2.0f, 1, 60 });
        woodenAxe.set(WeaponProperty{ 3.0f, 1.6f });
        registry.RegisterItem(woodenAxe);

        ir::ItemDef stoneAxe = ir::MakeItemDef(
            275, "stone_axe", "Stone Axe", 1, false,
            ir::GetQuadItemMesh(1, 7), true
        );
        stoneAxe.set(ToolProperty{ 4.0f, 2, 132 });
        stoneAxe.set(WeaponProperty{ 4.0f, 1.6f });
        registry.RegisterItem(stoneAxe);

        ir::ItemDef ironAxe = ir::MakeItemDef(
            258, "iron_axe", "Iron Axe", 1, false,
            ir::GetQuadItemMesh(2, 7), true
        );
        ironAxe.set(ToolProperty{ 6.0f, 3, 251 });
        ironAxe.set(WeaponProperty{ 5.0f, 1.6f });
        registry.RegisterItem(ironAxe);

        ir::ItemDef diamondAxe = ir::MakeItemDef(
            279, "diamond_axe", "Diamond Axe", 1, false,
            ir::GetQuadItemMesh(3, 7), true
        );
        diamondAxe.set(ToolProperty{ 8.0f, 4, 1562 });
        diamondAxe.set(WeaponProperty{ 6.0f, 1.6f });
        registry.RegisterItem(diamondAxe);

        ir::ItemDef goldAxe = ir::MakeItemDef(
            286, "gold_axe", "Golden Axe", 1, false,
            ir::GetQuadItemMesh(4, 7), true
        );
        goldAxe.set(ToolProperty{ 12.0f, 1, 33 });
        goldAxe.set(WeaponProperty{ 3.0f, 1.6f });
        registry.RegisterItem(goldAxe);
    }

    // Axes
    {
        ir::ItemDef woodenAxe = ir::MakeItemDef(
            271, "wooden_axe", "Wooden Axe", 1, false,
            ir::GetQuadItemMesh(0, 7), true
        );
        woodenAxe.set(ToolProperty{ 2.0f, 1, 60 });
        woodenAxe.set(WeaponProperty{ 3.0f, 1.6f });
        registry.RegisterItem(woodenAxe);

        ir::ItemDef stoneAxe = ir::MakeItemDef(
            275, "stone_axe", "Stone Axe", 1, false,
            ir::GetQuadItemMesh(1, 7), true
        );
        stoneAxe.set(ToolProperty{ 4.0f, 2, 132 });
        stoneAxe.set(WeaponProperty{ 4.0f, 1.6f });
        registry.RegisterItem(stoneAxe);

        ir::ItemDef ironAxe = ir::MakeItemDef(
            258, "iron_axe", "Iron Axe", 1, false,
            ir::GetQuadItemMesh(2, 7), true
        );
        ironAxe.set(ToolProperty{ 6.0f, 3, 251 });
        ironAxe.set(WeaponProperty{ 5.0f, 1.6f });
        registry.RegisterItem(ironAxe);

        ir::ItemDef diamondAxe = ir::MakeItemDef(
            279, "diamond_axe", "Diamond Axe", 1, false,
            ir::GetQuadItemMesh(3, 7), true
        );
        diamondAxe.set(ToolProperty{ 8.0f, 4, 1562 });
        diamondAxe.set(WeaponProperty{ 6.0f, 1.6f });
        registry.RegisterItem(diamondAxe);

        ir::ItemDef goldAxe = ir::MakeItemDef(
            286, "gold_axe", "Golden Axe", 1, false,
            ir::GetQuadItemMesh(4, 7), true
        );
        goldAxe.set(ToolProperty{ 12.0f, 1, 33 });
        goldAxe.set(WeaponProperty{ 3.0f, 1.6f });
        registry.RegisterItem(goldAxe);
    }

}
