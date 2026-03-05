#include <ASCIICraft/game/Game.hpp>
#include <ASCIICraft/world/World.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/HLSLIncludes.hpp>

#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/engine/MipFilters.hpp>
#include <ASCIIgL/engine/Camera2D.hpp>

#include <glm/vec2.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <ASCIICraft/ecs/data/ItemIndex.hpp>

#include <ASCIICraft/gui/GuiMeshes.hpp>
#include <ASCIICraft/gui/screens/PlayHUDScreen.hpp>
#include <ASCIICraft/gui/screens/InventoryScreen.hpp>

// ecs components
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>

// shaders
#include <ASCIICraft/rendering/TerrainShaders.hpp>
#include <ASCIICraft/rendering/GUIShaders.hpp>

Game::Game()
    : gameState(GameState::Playing)
    , inputSystem(eventBus)
    , gameplayInputFilter(inputSystem)
    , movementSystem(registry, gameplayInputFilter, eventBus)
    , physicsSystem(registry)
    , renderSystem(registry)
    , cameraSystem(registry, gameplayInputFilter)
    , guiManager(registry, eventBus, inputSystem)
    , blockUpdateSystem(registry, eventBus)
    , placingSystem(registry, eventBus)
    , miningSystem(registry, eventBus)
    , playerFactory(registry)
    , shouldInternalExit(false)
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

    float lowLight = ASCIIgL::PaletteUtil::sRGB255_Luminance(glm::ivec3(20, 20, 20));
    float highLight = ASCIIgL::PaletteUtil::sRGB255_Luminance(glm::ivec3(222, 222, 222));

    // Monochrome palettes (dark→light along hue direction). Swap which one is used to change the look.
    ASCIIgL::MonochromePalette grayPalette(lowLight, highLight, glm::ivec3(205, 205, 205));
    ASCIIgL::MonochromePalette sepiaPalette(lowLight, highLight, glm::ivec3(200, 170, 130));
    ASCIIgL::MonochromePalette greenPalette(lowLight, highLight, glm::ivec3(80, 200, 100));
    ASCIIgL::MonochromePalette bluePalette(lowLight, highLight, glm::ivec3(120, 160, 220));
    ASCIIgL::MonochromePalette amberPalette(lowLight, highLight, glm::ivec3(220, 180, 100));

    ASCIIgL::MonochromePalette gamePalette = grayPalette;

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

    ASCIIgL::Renderer& renderer = ASCIIgL::Renderer::GetInst();
    renderer.SetBackgroundCol(gamePalette.GetRGB(8));
    renderer.SetWireframe(false);
    renderer.SetBackfaceCulling(true);
    renderer.SetCCW(true);
    renderer.SetDiagnosticsEnabled(true);

    ASCIIgL::Logger::Debug("Initializing renderer...");
    renderer.Initialize(true, 4);

    ASCIIgL::Renderer::GetInst().SetBlendEnabled(true);

    ASCIIgL::Logger::Debug("Initializing block states...");
    InitializeBlockStates();

    ASCIIgL::Logger::Debug("Initializing world...");
    InitializeWorld();

    ASCIIgL::Logger::Debug("Initializing player...");
    InitializePlayer();

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

    ASCIIgL::Logger::Info("ASCIICraft initialized successfully!");
    return true;
}

void Game::Run(std::function<bool()> shouldExternalExit) {
    if (!Initialize()) {
        ASCIIgL::Logger::Error("Failed to initialize game");
        return;
    }

    ASCIIgL::Logger::Info("Starting game loop...");

    ASCIIgL::Profiler::GetInst().SetEnabled(true);

    int frameCounter = 0;
    while (!shouldExternalExit() && !shouldInternalExit) {
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
}

void Game::Update() {
    ASCIIgL::Logger::Debug("Game::Update - state = " +
        std::to_string(static_cast<int>(gameState)));

    inputSystem.SetInputMode(guiManager.IsBlockingInput() ? input::InputMode::GUI : input::InputMode::Gameplay);
    inputSystem.Update();

    for ([[maybe_unused]] const auto& e : eventBus.view<events::ToggleInventoryEvent>()) {
        guiManager.ToggleInventoryScreen();
    }
    for ([[maybe_unused]] const auto& e : eventBus.view<events::QuitRequestedEvent>()) {
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

            guiManager.Update();

            ASCIIgL::Logger::Debug("Update: Running mining system");
            miningSystem.Update();

            ASCIIgL::Logger::Debug("Update: Running placement system");
            placingSystem.Update();

            ASCIIgL::Logger::Debug("Update: Running block update system");
            blockUpdateSystem.Update();

            ASCIIgL::Logger::Debug("Update: Running movement system");
            movementSystem.Update();
            ASCIIgL::Logger::Debug("Update: Running camera system");
            cameraSystem.Update();

            ASCIIgL::Logger::Debug("Update: Running physics system");
            physicsSystem.Update();

            ASCIIgL::Logger::Debug("Update: Running world update");
            world->Update();

            break;
        }

        case GameState::Exiting:
            ASCIIgL::Logger::Info("GameState::Exiting triggered. Stopping game loop.");
            shouldInternalExit = true;
            break;
    }
}

void Game::Render() {
    ASCIIgL::Logger::Debug("Game::Render called.");

    {
        ASCIIgL::PROFILE_SCOPE("Clear Px Buff/Begin GPU Frame");
        ASCIIgL::Screen::GetInst().ClearPixelBuffer();
        ASCIIgL::Renderer::GetInst().BeginGpuFrame();
    }

    // All GPU draws: 2D must be drawn after 3D so the GUI is on top (see RenderSystem::BatchAndDraw).
    switch (gameState) {
        case GameState::Playing:
            {
                ASCIIgL::PROFILE_SCOPE("Render.RenderPlaying");
                 RenderPlaying();
            }
            break;
    }

    // Execute queued GPU draws in two passes (opaque, then transparent)
    ASCIIgL::Renderer::GetInst().FlushDraws();

    {
        ASCIIgL::PROFILE_SCOPE("Render.EndGpuFrame");
        ASCIIgL::Renderer::GetInst().EndGpuFrame();
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

    // Clear libraries if we want to release all resources on shutdown
    if (auto world = GetWorldPtr(registry)) {
        if (auto cm = world->GetChunkManager()) {
            cm->SaveAll();
        }
    }
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
    
    // Create gradient mapping shader program
    auto terrainVS = ASCIIgL::Shader::CreateFromSource(
        TerrainShaders::GetTerrainVSSource(),
        ASCIIgL::ShaderType::Vertex
    );
    
    ASCIIgL::ShaderIncludeMap terrainIncludes;
    ASCIIgL::HLSLIncludes::AddToMap(terrainIncludes);
    auto terrainPS = ASCIIgL::Shader::CreateFromSource(
        TerrainShaders::GetTerrainPSSource(),
        ASCIIgL::ShaderType::Pixel,
        "main",
        &terrainIncludes
    );
    
    if (!terrainVS || !terrainVS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile gradient map vertex shader: " + terrainVS->GetCompileError());
        return false;
    }
    
    if (!terrainPS || !terrainPS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile gradient map pixel shader: " + terrainPS->GetCompileError());
        return false;
    }
    
    // Create shader program with gradient map uniform layout (PosUVLayerLight = terrain + per-vertex light)
    auto blockShaderProgram = ASCIIgL::ShaderProgram::Create(
        std::move(terrainVS),
        std::move(terrainPS),
        ASCIIgL::VertFormats::PosUVLayerLight(),
        TerrainShaders::GetTerrainPSUniformLayout()
    );
    
    if (!blockShaderProgram || !blockShaderProgram->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create gradient map shader program");
        return false;
    }

    auto blockMaterial = ASCIIgL::Material::Create(std::move(blockShaderProgram));

    
    if (!blockMaterial) {
        ASCIIgL::Logger::Error("Failed to create block material");
        return false;
    }
    
    // Set texture array (weak reference inside Material, owned by TextureLibrary)
    blockMaterial->SetTextureArray(0, blockTextureArray.get());
    
    // Set gradient colors from palette entries with smallest and largest luminance (matches monochrome LUT)
    auto& palette = ASCIIgL::Screen::GetInst().GetPalette();
    blockMaterial->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(palette.GetMinLumIdx()), 1.0f));
    blockMaterial->SetFloat4("gradientEnd", glm::vec4(palette.GetRGBNormalized(palette.GetMaxLumIdx()), 1.0f));

    // Register material
    ASCIIgL::MaterialLibrary::GetInst().Register("blockMaterial", std::move(blockMaterial));

    auto inventoryTexture = ASCIIgL::TextureLibrary::GetInst().LoadTexture("res/textures/gui/inventory.png", "inventoryTexture");
    if (!inventoryTexture) {
        ASCIIgL::Logger::Error("Failed to load inventory texture");
        return false;
    }

    auto guiVS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetGUIVSSource(),
        ASCIIgL::ShaderType::Vertex
    );

    ASCIIgL::ShaderIncludeMap guiIncludes;
    ASCIIgL::HLSLIncludes::AddToMap(guiIncludes);
    auto guiPS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetGUIPSSource(),
        ASCIIgL::ShaderType::Pixel,
        "main",
        &guiIncludes
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

    auto guiMaterial = ASCIIgL::Material::Create(std::move(guiShaderProgram));

    if (!guiMaterial) {
        ASCIIgL::Logger::Error("Failed to create GUI material");
        return false;
    }

    {
        auto& palette = ASCIIgL::Screen::GetInst().GetPalette();
        guiMaterial->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(palette.GetMinLumIdx()), 1.0f));
        guiMaterial->SetFloat4("gradientEnd", glm::vec4(palette.GetRGBNormalized(palette.GetMaxLumIdx()), 1.0f));
    }
    // Register material (textures are set per-item via AddGuiItem texture parameter)
    ASCIIgL::MaterialLibrary::GetInst().Register("guiMaterial", std::move(guiMaterial));

    // GUI item material: PosUVLayerLight + texture array for item icons in slots
    auto itemVS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetItemVSSource(),
        ASCIIgL::ShaderType::Vertex
    );

    ASCIIgL::ShaderIncludeMap itemIncludes;
    ASCIIgL::HLSLIncludes::AddToMap(itemIncludes);
    auto itemPS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetItemPSSource(),
        ASCIIgL::ShaderType::Pixel,
        "main",
        &itemIncludes
    );

    if (!itemVS || !itemVS->IsValid() || !itemPS || !itemPS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile GUI item shaders");
        return false;
    }
    
    auto guiItemShaderProgram = ASCIIgL::ShaderProgram::Create(
        std::move(itemVS),
        std::move(itemPS),
        ASCIIgL::VertFormats::PosUVLayerLight(),
        GUIShaders::GetItemPSUniformLayout()
    );

    if (!guiItemShaderProgram || !guiItemShaderProgram->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create GUI item shader program");
        return false;
    }

    auto guiItemMaterial = ASCIIgL::Material::Create(std::move(guiItemShaderProgram));
    if (!guiItemMaterial) {
        ASCIIgL::Logger::Error("Failed to create GUI item material");
        return false;
    }

    {
        auto& palette = ASCIIgL::Screen::GetInst().GetPalette();
        guiItemMaterial->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(palette.GetMinLumIdx()), 1.0f));
        guiItemMaterial->SetFloat4("gradientEnd", glm::vec4(palette.GetRGBNormalized(palette.GetMaxLumIdx()), 1.0f));
    }
    auto* terrainTextureArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray").get();
    guiItemMaterial->SetTextureArray(0, terrainTextureArray);
    ASCIIgL::MaterialLibrary::GetInst().Register("guiItemMaterial", std::move(guiItemMaterial));

    // OOP GUI: create play + inventory screens after textures/materials exist (CreateQuadMesh needs terrainTextureArray)
    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player != entt::null) {
        auto guiQuad = gui::CreateQuadMesh();
        guiManager.SetPlayScreen(std::make_unique<gui::PlayHUDScreen>(
            registry, eventBus, player));
        auto inventoryTexture = ASCIIgL::TextureLibrary::GetInst().GetTexture("inventoryTexture");
        if (!inventoryTexture)
            ASCIIgL::Logger::Warning("LoadResources: inventoryTexture is null; inventory GUI will have no texture");
        guiManager.SetInventoryScreen(std::make_unique<gui::InventoryScreen>(
            registry, eventBus, player, guiQuad, inventoryTexture));
    }

    ASCIIgL::Logger::Info("Resources loaded successfully");
    return true;
}

void Game::RenderPlaying() {
    ASCIIgL::Logger::Debug("RenderPlaying: binding shader and texture array");

    // Keep 2D GUI camera in sync with viewport (GPU pipeline uses Screen dimensions; 2D ortho must match)
    guiManager.SetScreenSize(glm::vec2(
        static_cast<float>(ASCIIgL::Screen::GetInst().GetWidth()),
        static_cast<float>(ASCIIgL::Screen::GetInst().GetHeight())));

    GetWorldPtr(registry)->Render();

    renderSystem.BeginFrame();
    guiManager.Draw(renderSystem);
    renderSystem.SetActive2DCamera(guiManager.GetCamera2D());
    renderSystem.Render();
}

void Game::InitializeWorld() {
    registry.ctx().emplace<std::unique_ptr<World>>(std::make_unique<World>(registry, WorldCoord(0, 120, 0), 10));
    ASCIIgL::Logger::Debug("World created and stored in registry context.");
}

void Game::InitializePlayer() {
    playerFactory.createPlayerEnt(GetWorldPtr(registry)->GetSpawnPoint().ToVec3(), GameMode::Survival);
    ASCIIgL::Logger::Debug("Player entity created");
}

void Game::InitializeSystems() {
    ASCIIgL::Logger::Debug("Initializing systems...");

    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player != entt::null) {
        renderSystem.SetActive3DCamera(registry.try_get<ecs::components::PlayerCamera>(player));
    }

    guiManager.SetScreenSize(glm::vec2(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT)));

    ASCIIgL::Logger::Debug("Systems initialized.");
}

void Game::InitializeBlockStates() {
    auto& bsr = registry.ctx().emplace<blockstate::BlockStateRegistry>();

    constexpr int A = 16; // Atlas tile count
    auto L = [](int x, int y) { return ASCIIgL::TextureArray::GetLayerFromAtlasXY(x, y, 16); };

    // Helper: set all 6 faces to the same layer
    auto allFaces = [](blockstate::BlockState& s, int layer) {
        for (int i = 0; i < BLOCK_FACE_COUNT; ++i) s.faceTextureLayers[i] = layer;
    };

    // ======================== REGISTRATION ORDER ========================
    // Air MUST be first (typeId=0, stateId=0)
    bsr.RegisterType("minecraft:air", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:air"), [](blockstate::BlockState& s) {
        s.isSolid = false;
        s.isTransparent = true;
        s.renderMode = blockstate::RenderMode::Translucent;
    });

    // === Terrain ===
    bsr.RegisterType("minecraft:bedrock", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:bedrock"), [&](blockstate::BlockState& s) {
        allFaces(s, L(1, 1));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:stone", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:stone"), [&](blockstate::BlockState& s) {
        allFaces(s, L(1, 0));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:cobblestone", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:cobblestone"), [&](blockstate::BlockState& s) {
        allFaces(s, L(0, 1));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:dirt", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:dirt"), [&](blockstate::BlockState& s) {
        allFaces(s, L(2, 0));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:grass", {
        // BlockStateRegistry properties are string-based; use BlockFace in code and convert at placement time.
        blockstate::BlockProperty{ "facing", {"north", "south", "east", "west"}, 0 }
    });

    bsr.SetDerivedData(bsr.GetTypeId("minecraft:grass"), [&](blockstate::BlockState& s) {
        s.faceTextureLayers[0] = L(0, 0); // Top
        s.faceTextureLayers[1] = L(2, 0); // Bottom (dirt)
        s.faceTextureLayers[2] = L(3, 0); // North
        s.faceTextureLayers[3] = L(3, 0); // South
        s.faceTextureLayers[4] = L(3, 0); // East
        s.faceTextureLayers[5] = L(3, 0); // West
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:gravel", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:gravel"), [&](blockstate::BlockState& s) {
        allFaces(s, L(3, 1));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:sand", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:sand"), [&](blockstate::BlockState& s) {
        allFaces(s, L(2, 1));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:sandstone", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:sandstone"), [&](blockstate::BlockState& s) {
        allFaces(s, L(2, 2));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:clay", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:clay"), [&](blockstate::BlockState& s) {
        allFaces(s, L(4, 1));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    // === Ores ===
    bsr.RegisterType("minecraft:coal_ore", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:coal_ore"), [&](blockstate::BlockState& s) {
        allFaces(s, L(2, 2));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:iron_ore", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:iron_ore"), [&](blockstate::BlockState& s) {
        allFaces(s, L(1, 2));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:gold_ore", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:gold_ore"), [&](blockstate::BlockState& s) {
        allFaces(s, L(0, 2));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:diamond_ore", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:diamond_ore"), [&](blockstate::BlockState& s) {
        allFaces(s, L(3, 2));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    // === Wood & Plants ===
    bsr.RegisterType("minecraft:oak_log", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:oak_log"), [&](blockstate::BlockState& s) {
        s.faceTextureLayers[0] = L(5, 1); // Top (rings)
        s.faceTextureLayers[1] = L(5, 1); // Bottom (rings)
        s.faceTextureLayers[2] = L(4, 1); // Sides (bark)
        s.faceTextureLayers[3] = L(4, 1);
        s.faceTextureLayers[4] = L(4, 1);
        s.faceTextureLayers[5] = L(4, 1);
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:oak_leaves", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:oak_leaves"), [&](blockstate::BlockState& s) {
        allFaces(s, L(4, 3));
        s.isSolid = true;
        s.isTransparent = false;                  // treat as cutout, not blended
        s.renderMode = blockstate::RenderMode::Cutout;
    });

    bsr.RegisterType("minecraft:oak_planks", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:oak_planks"), [&](blockstate::BlockState& s) {
        allFaces(s, L(4, 0));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:spruce_log", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:spruce_log"), [&](blockstate::BlockState& s) {
        s.faceTextureLayers[0] = L(5, 1);
        s.faceTextureLayers[1] = L(5, 1);
        s.faceTextureLayers[2] = L(4, 1);
        s.faceTextureLayers[3] = L(4, 1);
        s.faceTextureLayers[4] = L(4, 1);
        s.faceTextureLayers[5] = L(4, 1);
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:spruce_leaves", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:spruce_leaves"), [&](blockstate::BlockState& s) {
        allFaces(s, L(5, 3));
        s.isSolid = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
    });

    bsr.RegisterType("minecraft:spruce_planks", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:spruce_planks"), [&](blockstate::BlockState& s) {
        allFaces(s, L(5, 0));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    // === Utility Blocks ===
    bsr.RegisterType("minecraft:crafting_table", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:crafting_table"), [&](blockstate::BlockState& s) {
        s.faceTextureLayers[0] = L(11, 2); // Top
        s.faceTextureLayers[1] = L(4, 0);  // Bottom (planks)
        s.faceTextureLayers[2] = L(11, 3); // North
        s.faceTextureLayers[3] = L(11, 3); // South
        s.faceTextureLayers[4] = L(12, 3); // East
        s.faceTextureLayers[5] = L(12, 3); // West
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:furnace", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:furnace"), [&](blockstate::BlockState& s) {
        s.faceTextureLayers[0] = L(14, 3); // Top
        s.faceTextureLayers[1] = L(14, 3); // Bottom
        s.faceTextureLayers[2] = L(12, 2); // North (front)
        s.faceTextureLayers[3] = L(13, 2); // South (side)
        s.faceTextureLayers[4] = L(13, 2); // East (side)
        s.faceTextureLayers[5] = L(13, 2); // West (side)
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    // === Special Blocks ===
    bsr.RegisterType("minecraft:tnt", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:tnt"), [&](blockstate::BlockState& s) {
        allFaces(s, L(8, 0));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:obsidian", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:obsidian"), [&](blockstate::BlockState& s) {
        allFaces(s, L(5, 2));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:mossy_cobblestone", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:mossy_cobblestone"), [&](blockstate::BlockState& s) {
        allFaces(s, L(0, 3));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:bookshelf", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:bookshelf"), [&](blockstate::BlockState& s) {
        s.faceTextureLayers[0] = L(4, 0); // Top (planks)
        s.faceTextureLayers[1] = L(4, 0); // Bottom (planks)
        s.faceTextureLayers[2] = L(3, 3); // Sides
        s.faceTextureLayers[3] = L(3, 3);
        s.faceTextureLayers[4] = L(3, 3);
        s.faceTextureLayers[5] = L(3, 3);
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    bsr.RegisterType("minecraft:wool", {});
    bsr.SetDerivedData(bsr.GetTypeId("minecraft:wool"), [&](blockstate::BlockState& s) {
        allFaces(s, L(0, 4));
        s.renderMode = blockstate::RenderMode::Opaque;
    });

    ASCIIgL::Logger::Info("BlockStateRegistry: " +
        std::to_string(bsr.GetTotalTypeCount()) + " types, " +
        std::to_string(bsr.GetTotalStateCount()) + " states registered.");
}

void Game::InitializeItemDefinitions() {
    auto& itemIndex = registry.ctx().emplace<ecs::data::ItemIndex>();

    using IX = ecs::data::ItemIndex;
    auto& bsr = registry.ctx().get<blockstate::BlockStateRegistry>();
    auto blockMesh = [&](const std::string& typeName) {
        return IX::GetBlockMeshFromState(bsr.GetState(bsr.GetDefaultState(typeName)));
    };

    // === Terrain === (IDs auto-assigned: 1, 2, 3, ...)
    itemIndex.RegisterBlockItem(registry, "minecraft:stone",       "Stone",       blockMesh("minecraft:stone"));
    itemIndex.RegisterBlockItem(registry, "minecraft:cobblestone", "Cobblestone", blockMesh("minecraft:cobblestone"));
    itemIndex.RegisterBlockItem(registry, "minecraft:dirt",        "Dirt",        blockMesh("minecraft:dirt"));
    itemIndex.RegisterBlockItem(registry, "minecraft:grass",       "Grass Block", blockMesh("minecraft:grass"));
    itemIndex.RegisterBlockItem(registry, "minecraft:gravel",      "Gravel",      blockMesh("minecraft:gravel"));
    itemIndex.RegisterBlockItem(registry, "minecraft:sand",        "Sand",        blockMesh("minecraft:sand"));
    itemIndex.RegisterBlockItem(registry, "minecraft:sandstone",   "Sandstone",   blockMesh("minecraft:sandstone"));
    itemIndex.RegisterBlockItem(registry, "minecraft:clay",        "Clay",        blockMesh("minecraft:clay"));
    itemIndex.RegisterBlockItem(registry, "minecraft:bedrock",     "Bedrock",     blockMesh("minecraft:bedrock"));

    // === Ores ===
    itemIndex.RegisterBlockItem(registry, "minecraft:coal_ore",    "Coal Ore",    blockMesh("minecraft:coal_ore"));
    itemIndex.RegisterBlockItem(registry, "minecraft:iron_ore",    "Iron Ore",    blockMesh("minecraft:iron_ore"));
    itemIndex.RegisterBlockItem(registry, "minecraft:gold_ore",    "Gold Ore",    blockMesh("minecraft:gold_ore"));
    itemIndex.RegisterBlockItem(registry, "minecraft:diamond_ore", "Diamond Ore", blockMesh("minecraft:diamond_ore"));

    // === Wood & Plants ===
    itemIndex.RegisterBlockItem(registry, "minecraft:oak_log",       "Oak Log",       blockMesh("minecraft:oak_log"));
    itemIndex.RegisterBlockItem(registry, "minecraft:oak_leaves",    "Oak Leaves",    blockMesh("minecraft:oak_leaves"));
    itemIndex.RegisterBlockItem(registry, "minecraft:oak_planks",    "Oak Planks",    blockMesh("minecraft:oak_planks"));
    itemIndex.RegisterBlockItem(registry, "minecraft:spruce_log",    "Spruce Log",    blockMesh("minecraft:spruce_log"));
    itemIndex.RegisterBlockItem(registry, "minecraft:spruce_leaves", "Spruce Leaves", blockMesh("minecraft:spruce_leaves"));
    itemIndex.RegisterBlockItem(registry, "minecraft:spruce_planks", "Spruce Planks", blockMesh("minecraft:spruce_planks"));

    // === Utility Blocks ===
    itemIndex.RegisterBlockItem(registry, "minecraft:crafting_table", "Crafting Table", blockMesh("minecraft:crafting_table"));
    itemIndex.RegisterBlockItem(registry, "minecraft:furnace",        "Furnace",        blockMesh("minecraft:furnace"));

    // === Special Blocks ===
    itemIndex.RegisterBlockItem(registry, "minecraft:tnt",                "TNT",                blockMesh("minecraft:tnt"));
    itemIndex.RegisterBlockItem(registry, "minecraft:obsidian",           "Obsidian",           blockMesh("minecraft:obsidian"));
    itemIndex.RegisterBlockItem(registry, "minecraft:mossy_cobblestone",  "Mossy Cobblestone",  blockMesh("minecraft:mossy_cobblestone"));
    itemIndex.RegisterBlockItem(registry, "minecraft:bookshelf",          "Bookshelf",          blockMesh("minecraft:bookshelf"));
    itemIndex.RegisterBlockItem(registry, "minecraft:wool",               "Wool",               blockMesh("minecraft:wool"));

    // === Resources / Materials ===
    itemIndex.RegisterResourceItem(registry, 263, "minecraft:coal",       "Coal",       IX::GetQuadItemMesh(7, 10));
    itemIndex.RegisterResourceItem(registry, 264, "minecraft:diamond",    "Diamond",    IX::GetQuadItemMesh(8, 10));
    itemIndex.RegisterResourceItem(registry, 265, "minecraft:iron_ingot", "Iron Ingot", IX::GetQuadItemMesh(9, 10));
    itemIndex.RegisterResourceItem(registry, 266, "minecraft:gold_ingot", "Gold Ingot", IX::GetQuadItemMesh(10, 10));
    itemIndex.RegisterResourceItem(registry, 280, "minecraft:stick",      "Stick",      IX::GetQuadItemMesh(5, 9));

    // === Swords ===
    itemIndex.RegisterToolItem(registry, 268, "minecraft:wooden_sword",  "Wooden Sword",  IX::GetQuadItemMesh(0, 4), {2.0f, 1, 60},   {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 272, "minecraft:stone_sword",   "Stone Sword",   IX::GetQuadItemMesh(1, 4), {4.0f, 2, 132},  {5.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 267, "minecraft:iron_sword",    "Iron Sword",    IX::GetQuadItemMesh(2, 4), {6.0f, 3, 251},  {6.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 276, "minecraft:diamond_sword", "Diamond Sword", IX::GetQuadItemMesh(3, 4), {8.0f, 4, 1562}, {7.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 283, "minecraft:gold_sword",    "Golden Sword",  IX::GetQuadItemMesh(4, 4), {12.0f, 1, 33},  {4.0f, 1.6f});

    // === Shovels ===
    itemIndex.RegisterToolItem(registry, 269, "minecraft:wooden_shovel",  "Wooden Shovel",  IX::GetQuadItemMesh(0, 5), {2.0f, 1, 60},   {1.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 273, "minecraft:stone_shovel",   "Stone Shovel",   IX::GetQuadItemMesh(1, 5), {4.0f, 2, 132},  {2.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 256, "minecraft:iron_shovel",    "Iron Shovel",    IX::GetQuadItemMesh(2, 5), {6.0f, 3, 251},  {3.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 277, "minecraft:diamond_shovel", "Diamond Shovel", IX::GetQuadItemMesh(3, 5), {8.0f, 4, 1562}, {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 284, "minecraft:gold_shovel",    "Golden Shovel",  IX::GetQuadItemMesh(4, 5), {12.0f, 1, 33},  {1.0f, 1.6f});

    // === Pickaxes ===
    itemIndex.RegisterToolItem(registry, 270, "minecraft:wooden_pickaxe",  "Wooden Pickaxe",  IX::GetQuadItemMesh(0, 6), {2.0f, 1, 60},   {2.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 274, "minecraft:stone_pickaxe",   "Stone Pickaxe",   IX::GetQuadItemMesh(1, 6), {4.0f, 2, 132},  {3.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 257, "minecraft:iron_pickaxe",    "Iron Pickaxe",    IX::GetQuadItemMesh(2, 6), {6.0f, 3, 251},  {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 278, "minecraft:diamond_pickaxe", "Diamond Pickaxe", IX::GetQuadItemMesh(3, 6), {8.0f, 4, 1562}, {5.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 285, "minecraft:gold_pickaxe",    "Golden Pickaxe",  IX::GetQuadItemMesh(4, 6), {12.0f, 1, 33},  {2.0f, 1.6f});

    // === Axes ===
    itemIndex.RegisterToolItem(registry, 271, "minecraft:wooden_axe",  "Wooden Axe",  IX::GetQuadItemMesh(0, 7), {2.0f, 1, 60},   {3.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 275, "minecraft:stone_axe",   "Stone Axe",   IX::GetQuadItemMesh(1, 7), {4.0f, 2, 132},  {4.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 258, "minecraft:iron_axe",    "Iron Axe",    IX::GetQuadItemMesh(2, 7), {6.0f, 3, 251},  {5.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 279, "minecraft:diamond_axe", "Diamond Axe", IX::GetQuadItemMesh(3, 7), {8.0f, 4, 1562}, {6.0f, 1.6f});
    itemIndex.RegisterToolItem(registry, 286, "minecraft:gold_axe",    "Golden Axe",  IX::GetQuadItemMesh(4, 7), {12.0f, 1, 33},  {3.0f, 1.6f});
}
