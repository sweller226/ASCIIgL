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
#include <ASCIIgL/engine/MonochromeMapping.hpp>
#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/Profiler.hpp>

#include <glm/vec2.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/JsonBlockModelRegistration.hpp>
#include <ASCIICraft/world/block/models/JsonModelLoader.hpp>
#include <ASCIICraft/world/block/state/JsonBlockStateLoader.hpp>
#include <ASCIICraft/world/block/state/VariantKey.hpp>
#include <ASCIICraft/world/block/textures/BlockTextureCatalog.hpp>
#include <ASCIICraft/world/block/models/WaterModelBuilder.hpp>
#include <ASCIICraft/ecs/data/ItemIndex.hpp>

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
    , ecsRenderSystem(registry)
    , cameraSystem(registry, gameplayInputFilter)
    , guiManager(registry, eventBus, inputSystem)
    , blockUpdateSystem(registry, eventBus)
    , placingSystem(registry, eventBus)
    , miningSystem(registry, eventBus)
    , lifetimeSystem(registry)
    , particleSystem(registry, eventBus)
    , soundSystem(registry, eventBus)
    , playerFactory(registry)
    , shouldInternalExit(false)
{
    ASCIIgL::Logger::Debug("Game constructor: systems created, registry bound.");
}

Game::~Game() {
    ASCIIgL::Logger::Debug("Game destructor called.");
    Shutdown();
}

bool Game::Initialize(bool renderToTerminal) {
    ASCIIgL::Logger::Info("Initializing ASCIICraft...");

    ASCIIgL::Logger::Debug("Preloading textures for palette generation...");

    LoadTextures();

    std::vector<std::pair<float, std::shared_ptr<ASCIIgL::Texture>>> textureWeights = {
        {0.0f, ASCIIgL::TextureLibrary::GetInst().GetTexture("inventoryTexture")}
    };

    std::vector<std::pair<float, std::shared_ptr<ASCIIgL::TextureArray>>> textureArrayWeights = {
        {1.0f, ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray")}
    };

    ASCIIgL::MonochromePalette gamePalette(textureWeights, textureArrayWeights);

    ASCIIgL::Logger::Debug("Initializing screen...");
    if (ASCIIgL::Screen::GetInst().Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, L"ASCIICraft", FONT_SIZE, gamePalette, renderToTerminal) != 0) {
        ASCIIgL::Logger::Error("Failed to initialize screen");
        return false;
    }

    SCREEN_WIDTH = ASCIIgL::Screen::GetInst().GetWidth();
    SCREEN_HEIGHT = ASCIIgL::Screen::GetInst().GetHeight();
    ASCIIgL::Logger::Debug("Screen initialized: " + std::to_string(SCREEN_WIDTH) + "x" + std::to_string(SCREEN_HEIGHT));

    guiCamera = std::make_unique<ASCIIgL::Camera2D>(glm::vec2(0, 0), SCREEN_WIDTH, SCREEN_HEIGHT);

    ASCIIgL::FPSClock::GetInst().Initialize(static_cast<unsigned int>(TARGET_FPS), 1.0f);
    ASCIIgL::Logger::Debug("FPSClock initialized with target FPS: " + std::to_string(TARGET_FPS));

    ASCIIgL::Renderer& renderer = ASCIIgL::Renderer::GetInst();
    renderer.SetMonochromeDitherEnabled(true);
    renderer.SetBackgroundCol(glm::ivec3(255, 255, 255));
    renderer.SetWireframe(false);
    renderer.SetBackfaceCulling(true);
    renderer.SetCCW(true);

    ASCIIgL::Logger::Debug("Initializing renderer...");
    renderer.Initialize(true, 4, nullptr, 10);

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

void Game::Run(std::function<bool()> shouldExternalExit, bool renderToTerminal) {
    if (!Initialize(renderToTerminal)) {
        ASCIIgL::Logger::Error("Failed to initialize game");
        return;
    }

    ASCIIgL::Logger::Info("Starting game loop...");

    int frameCounter = 0;
    while (!shouldExternalExit() && !shouldInternalExit) {
        ASCIIgL::Screen::GetInst().ProcessMessages();
        if (ASCIIgL::Screen::GetInst().ShouldExit())
            break;

        PROFILE_FRAME_MARK();
        ASCIIgL::FPSClock::GetInst().StartFPSClock();

        {
            PROFILE_SCOPE("Update");
            Update();
        }

        {
            PROFILE_SCOPE("RenderGame");
            Render();
        }

        eventBus.endFrame();

        ASCIIgL::FPSClock::GetInst().EndFPSClock();
        frameCounter++;

        if (frameCounter % 60 == 0) {
            ASCIIgL::Logger::Info("FPS: " + std::to_string(ASCIIgL::FPSClock::GetInst().GetFPS()));
            frameCounter = 0;
        }
    }   
}

void Game::Update() {
    const bool guiBlocking = guiManager.IsBlockingInput();
    const auto mode = guiBlocking ? input::InputMode::GUI : input::InputMode::Gameplay;
    inputSystem.SetInputMode(mode);
    inputSystem.Update();

    for ([[maybe_unused]] const auto& e : eventBus.view<events::ToggleInventoryEvent>()) {
        // Temporary behavior: allow close-only; ignore requests that would open inventory.
    }
    for ([[maybe_unused]] const auto& e : eventBus.view<events::QuitRequestedEvent>()) {
        ASCIIgL::Logger::Info("Quit action detected. Exiting game...");
        gameState = GameState::Exiting;
    }

    switch (gameState) {
        case GameState::Playing: {

            World* world = GetWorldPtr(registry);

            if (!world) {
                ASCIIgL::Logger::Error("Update aborted: World pointer is null");
                return;
            }

            lifetimeSystem.Update();

            guiManager.Update();

            miningSystem.Update();
            placingSystem.Update();
            blockUpdateSystem.Update();
            
            particleSystem.Update();

            movementSystem.Update();
            cameraSystem.Update();
            physicsSystem.Update();

            soundSystem.Update();

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

    {
        PROFILE_SCOPE("Clear Px Buff/Begin GPU Frame");
        ASCIIgL::Screen::GetInst().ClearPixelBuffer();
        ASCIIgL::Renderer::GetInst().BeginGpuFrame();
    }

    // All GPU draws: 2D must be drawn after 3D so the GUI is on top (see RenderSystem::BatchAndDraw).
    switch (gameState) {
        case GameState::Playing:
            {
                PROFILE_SCOPE("Render.RenderPlaying");
                 RenderPlaying();
            }
            break;
    }

    // Execute queued GPU draws in two passes (opaque, then transparent)
    {
        PROFILE_SCOPE("Render.FlushDraws");
        ASCIIgL::Renderer::GetInst().FlushDraws();  
    }

    {
        PROFILE_SCOPE("Render.EndGpuFrame");
        ASCIIgL::Renderer::GetInst().EndGpuFrame();
    }

    {
        PROFILE_SCOPE("Render.PixelBufferDraws");
        ASCIIgL::Renderer::GetInst().DrawScreenBorderPxBuff(0xF);
    }

    {
        PROFILE_SCOPE("Render.PixelBufferOutput");
        ASCIIgL::Screen::GetInst().OutputBuffer();
    }
}

void Game::Shutdown() {
    if (shutdownInvoked_) {
        return;
    }
    shutdownInvoked_ = true;

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

bool Game::LoadTextures() {
    ASCIIgL::Logger::Info("Loading game textures...");

    // Alternative monochrome hue directions (console palette indices 0â€“15 per channel).
    const glm::ivec3 NeutralGrayHue   = glm::ivec3(14, 14, 14); // balanced gray
    const glm::ivec3 WarmSepiaHue     = glm::ivec3(15, 12, 8);  // warm brownish
    const glm::ivec3 CoolBlueHue      = glm::ivec3(10, 10, 15); // bluish cool
    const glm::ivec3 EmeraldGreenHue  = glm::ivec3(10, 15, 12); // green/cyan tilt
    const glm::ivec3 ElectricMagentaHue = glm::ivec3(15, 10, 15); // magenta/purple

    float darkL  = ASCIIgL::PaletteUtil::sRGB255_Luminance(glm::ivec3(30, 30, 30));
    float lightL = ASCIIgL::PaletteUtil::sRGB255_Luminance(glm::ivec3(212, 212, 212));

    // Build monochrome mapping from the current screen palette if possible.
    ASCIIgL::MonochromeMapping monoMap;
    monoMap.enabled = true;
    monoMap.darkL = darkL;
    monoMap.lightL = lightL;
    monoMap.hueDir = NeutralGrayHue;
    monoMap.brightness = 1.0f;
    monoMap.contrast = 1.0f;

    // Load block textures from central catalog order.
    std::vector<std::string> blockTexturePaths = blocktextures::BuildBlockTexturePaths();

    auto blockTextureArray = ASCIIgL::TextureLibrary::GetInst().LoadTextureArray(blockTexturePaths, "terrainTextureArray", monoMap);
    if (!blockTextureArray || !blockTextureArray->IsValid()) {
        ASCIIgL::Logger::Error("Failed to load block texture array");
        return false;   
    }

    auto inventoryTexture = ASCIIgL::TextureLibrary::GetInst().LoadTexture("res/textures/gui/container/inventory.png", "inventoryTexture", monoMap);
    if (!inventoryTexture) {
        ASCIIgL::Logger::Error("Failed to load inventory texture");
        return false;
    }

    constexpr int kFontGlyphTileSize = 8; // Minecraft default font atlas uses 8x8 glyph cells.
    auto fontTextureArray = ASCIIgL::TextureLibrary::GetInst().LoadTextureArray(
        "res/font/default.png",
        kFontGlyphTileSize,
        "defaultFontTextureArray",
        monoMap
    );
    if (!fontTextureArray || !fontTextureArray->IsValid()) {
        ASCIIgL::Logger::Error("Failed to load default bitmap font texture atlas");
        return false;
    }

    return true;
}

bool Game::LoadResources() {
    ASCIIgL::Logger::Info("Loading game resources...");

    if (!LoadTerrainMaterial())      return false;
    if (!LoadGUIMaterial())          return false;
    if (!LoadGUIItemMaterial())      return false;

    ASCIIgL::Logger::Info("Resources loaded successfully");
    return true;
}

bool Game::LoadTerrainMaterial() {
    auto terrainVS = ASCIIgL::Shader::CreateFromSource(TerrainShaders::GetTerrainVSSource(), ASCIIgL::ShaderType::Vertex);

    ASCIIgL::ShaderIncludeMap includes;
    ASCIIgL::HLSLIncludes::AddToMap(includes);
    auto terrainPS = ASCIIgL::Shader::CreateFromSource(TerrainShaders::GetTerrainPSSource(), ASCIIgL::ShaderType::Pixel, "main", &includes);

    if (!terrainVS || !terrainVS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile terrain vertex shader: " + terrainVS->GetCompileError());
        return false;
    }
    if (!terrainPS || !terrainPS->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile terrain pixel shader: " + terrainPS->GetCompileError());
        return false;
    }

    auto program = ASCIIgL::ShaderProgram::Create(
        std::move(terrainVS), std::move(terrainPS),
        ASCIIgL::VertFormats::PosUVLayerLight(),
        TerrainShaders::GetTerrainPSUniformLayout()
    );
    if (!program || !program->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create terrain shader program");
        return false;
    }

    auto material = ASCIIgL::Material::Create(std::move(program));
    if (!material) {
        ASCIIgL::Logger::Error("Failed to create terrain material");
        return false;
    }

    material->SetTextureArray(0, ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray").get());

    auto& palette = ASCIIgL::Screen::GetInst().GetPalette();
    material->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(palette.GetMinLumIdx()), 1.0f));
    material->SetFloat4("gradientEnd",   glm::vec4(palette.GetRGBNormalized(palette.GetMaxLumIdx()), 1.0f));

    ASCIIgL::MaterialLibrary::GetInst().Register("blockMaterial", std::move(material));
    return true;
}

bool Game::LoadGUIMaterial() {
    ASCIIgL::ShaderIncludeMap includes;
    ASCIIgL::HLSLIncludes::AddToMap(includes);

    auto vs = ASCIIgL::Shader::CreateFromSource(GUIShaders::GetGUIVSSource(), ASCIIgL::ShaderType::Vertex);
    auto ps = ASCIIgL::Shader::CreateFromSource(GUIShaders::GetGUIPSSource(), ASCIIgL::ShaderType::Pixel, "main", &includes);

    if (!vs || !vs->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile GUI vertex shader: " + vs->GetCompileError());
        return false;
    }
    if (!ps || !ps->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile GUI pixel shader: " + ps->GetCompileError());
        return false;
    }

    auto program = ASCIIgL::ShaderProgram::Create(
        std::move(vs), std::move(ps),
        ASCIIgL::VertFormats::PosUV(),
        GUIShaders::GetGUIPSUniformLayout()
    );
    if (!program || !program->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create GUI shader program");
        return false;
    }

    auto material = ASCIIgL::Material::Create(std::move(program));
    if (!material) {
        ASCIIgL::Logger::Error("Failed to create GUI material");
        return false;
    }

    auto& palette = ASCIIgL::Screen::GetInst().GetPalette();
    material->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(palette.GetMinLumIdx()), 1.0f));
    material->SetFloat4("gradientEnd",   glm::vec4(palette.GetRGBNormalized(palette.GetMaxLumIdx()), 1.0f));

    ASCIIgL::MaterialLibrary::GetInst().Register("guiMaterial", std::move(material));
    return true;
}

bool Game::LoadGUIItemMaterial() {
    ASCIIgL::ShaderIncludeMap includes;
    ASCIIgL::HLSLIncludes::AddToMap(includes);

    auto vs = ASCIIgL::Shader::CreateFromSource(GUIShaders::GetItemVSSource(), ASCIIgL::ShaderType::Vertex);
    auto ps = ASCIIgL::Shader::CreateFromSource(GUIShaders::GetItemPSSource(), ASCIIgL::ShaderType::Pixel, "main", &includes);

    if (!vs || !vs->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile GUI item vertex shader: " + vs->GetCompileError());
        return false;
    }
    if (!ps || !ps->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile GUI item pixel shader: " + ps->GetCompileError());
        return false;
    }

    auto program = ASCIIgL::ShaderProgram::Create(
        std::move(vs), std::move(ps),
        ASCIIgL::VertFormats::PosUVLayerLight(),
        GUIShaders::GetItemPSUniformLayout()
    );
    if (!program || !program->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create GUI item shader program");
        return false;
    }

    auto material = ASCIIgL::Material::Create(std::move(program));
    if (!material) {
        ASCIIgL::Logger::Error("Failed to create GUI item material");
        return false;
    }

    auto& palette = ASCIIgL::Screen::GetInst().GetPalette();
    material->SetFloat4("gradientStart", glm::vec4(palette.GetRGBNormalized(palette.GetMinLumIdx()), 1.0f));
    material->SetFloat4("gradientEnd",   glm::vec4(palette.GetRGBNormalized(palette.GetMaxLumIdx()), 1.0f));

    ASCIIgL::MaterialLibrary::GetInst().Register("guiItemMaterial", std::move(material));
    return true;
}

void Game::RenderPlaying() {
    // Keep 2D GUI camera in sync with viewport (GPU pipeline uses Screen dimensions; 2D ortho must match)
    GetWorldPtr(registry)->Render();

    guiManager.Render();
    ecsRenderSystem.Render();
}

void Game::InitializeWorld() {
    WorldParams worldParams{};
    worldParams.spawnPoint = WorldCoord(0, 120, 0);
    worldParams.renderDistance = 10;
    worldParams.worldSeed = 12345ULL;
    registry.ctx().emplace<std::unique_ptr<World>>(std::make_unique<World>(registry, worldParams));
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
        ecsRenderSystem.SetActive3DCamera(registry.try_get<ecs::components::PlayerCamera>(player));
    }

    guiManager.SetActive2DCamera(guiCamera.get());

    particleSystem.Init();

    ASCIIgL::Logger::Debug("Systems initialized.");
}

void Game::InitializeBlockStates() {
    auto& bsr = registry.ctx().emplace<blockstate::BlockStateRegistry>();
    auto& modelLibrary = registry.ctx().emplace<blockmodels::BlockModelLibrary>();

    // ======================== Block model registration ========================
    // 1) Air first (typeId=0, stateId=0). 2) Shared JSON loaders for one assets root.
    // 3) Types + derived render data. 4) Models via JsonBlockModelRegistration (1.8.9 assets),
    //    except air (no mesh) and water (no blockstate in pack — procedural mesh).
    bsr.RegisterType("minecraft:air", {});
    const uint16_t airType = bsr.GetTypeId("minecraft:air");
    bsr.SetDerivedData(airType, [](blockstate::BlockState& s) {
        s.isRenderable = false;
        s.isTransparent = true;
        s.renderMode = blockstate::RenderMode::Translucent;
    });
    modelLibrary.RegisterModel(airType, nullptr, bsr);

    static constexpr const char* kVanillaBlockAssetRoot = "res";
    blockstate::JsonBlockStateLoader blockstateLoader(kVanillaBlockAssetRoot);
    blockmodels::JsonModelLoader jsonModelLoader(kVanillaBlockAssetRoot);

    const auto registerJsonBackedOrLog = [&](const char* typeName) {
        if (!blockstate::RegisterJsonBackedBlockType(
                typeName,
                bsr,
                modelLibrary,
                blockstateLoader,
                jsonModelLoader
            )) {
            ASCIIgL::Logger::Error(
                std::string("Game::InitializeBlockStates: JSON model registration failed for ") + typeName
            );
        }
    };

    const auto registerOpaqueJsonBacked = [&](const char* typeName) {
        bsr.RegisterType(typeName, {});
        const uint16_t tid = bsr.GetTypeId(typeName);
        bsr.SetDerivedData(tid, [](blockstate::BlockState& s) { s.renderMode = blockstate::RenderMode::Opaque; });
        registerJsonBackedOrLog(typeName);
    };

    // === Terrain & plants (vanilla blockstate + block models under res/blockstates and res/models) ===
    registerOpaqueJsonBacked("minecraft:bedrock");

    registerOpaqueJsonBacked("minecraft:stone");

    bsr.RegisterType("minecraft:dandelion", {});
    const uint16_t dandelionType = bsr.GetTypeId("minecraft:dandelion");
    bsr.SetDerivedData(dandelionType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:dandelion");

    bsr.RegisterType("minecraft:poppy", {});
    const uint16_t poppyType = bsr.GetTypeId("minecraft:poppy");
    bsr.SetDerivedData(poppyType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:poppy");

    bsr.RegisterType("minecraft:tall_grass", {});
    const uint16_t tallGrassType = bsr.GetTypeId("minecraft:tall_grass");
    bsr.SetDerivedData(tallGrassType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:tall_grass");

    bsr.RegisterType("minecraft:fern", {});
    const uint16_t fernType = bsr.GetTypeId("minecraft:fern");
    bsr.SetDerivedData(fernType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:fern");

    // Oak fence geometry in 1.8.9 uses blockstate id `minecraft:fence` (see assets/.../blockstates/fence.json).
    bsr.RegisterType("minecraft:fence", {
        blockstate::BlockProperty{ "east", { "false", "true" }},
        blockstate::BlockProperty{ "north", { "false", "true" }},
        blockstate::BlockProperty{ "south", { "false", "true" }},
        blockstate::BlockProperty{ "west", { "false", "true" }},
    });
    const uint16_t fenceType = bsr.GetTypeId("minecraft:fence");
    bsr.SetDerivedData(fenceType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:fence");

    registerOpaqueJsonBacked("minecraft:cobblestone");

    // 1.8.9 cobblestone stairs are represented by block id `stone_stairs`.
    bsr.RegisterType("minecraft:stone_stairs", {
        blockstate::BlockProperty{ "facing", { "east", "west", "south", "north" } },
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
        blockstate::BlockProperty{ "shape", { "straight", "outer_right", "outer_left", "inner_right", "inner_left" } },
    });
    const uint16_t stoneStairsType = bsr.GetTypeId("minecraft:stone_stairs");
    bsr.SetDerivedData(stoneStairsType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:stone_stairs");

    bsr.RegisterType("minecraft:cobblestone_slab", {
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
    });
    const uint16_t cobblestoneSlabType = bsr.GetTypeId("minecraft:cobblestone_slab");
    bsr.SetDerivedData(cobblestoneSlabType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:cobblestone_slab");

    registerOpaqueJsonBacked("minecraft:dirt");

    // Matches assets/minecraft/blockstates/grass.json (snowy + rotated grass_normal variants).
    bsr.RegisterType("minecraft:grass", {
        blockstate::BlockProperty{ "snowy", {"false", "true"}, 0 }
    });
    const uint16_t grassType = bsr.GetTypeId("minecraft:grass");
    bsr.SetDerivedData(grassType, [&](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:grass");

    // Matches assets/minecraft/blockstates/oak_log.json
    bsr.RegisterType("minecraft:oak_log", {
        blockstate::BlockProperty{ "axis", {"y", "z", "x", "none"}, 0 }
    });
    const uint16_t oakLogType = bsr.GetTypeId("minecraft:oak_log");
    bsr.SetDerivedData(oakLogType, [&](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:oak_log");

    // Matches assets/minecraft/blockstates/oak_planks.json
    registerOpaqueJsonBacked("minecraft:oak_planks");

    bsr.RegisterType("minecraft:oak_slab", {
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
    });
    const uint16_t oakSlabType = bsr.GetTypeId("minecraft:oak_slab");
    bsr.SetDerivedData(oakSlabType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:oak_slab");

    bsr.RegisterType("minecraft:oak_leaves", {});
    const uint16_t leavesType = bsr.GetTypeId("minecraft:oak_leaves");
    bsr.SetDerivedData(leavesType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;                  // treat as cutout, not blended
        s.renderMode = blockstate::RenderMode::Cutout;
        s.cullSameType = false;                   // draw faces between leaves (fuller look)
    });
    registerJsonBackedOrLog("minecraft:oak_leaves");

    // === Utility / decor blocks present in current texture array ===
    registerOpaqueJsonBacked("minecraft:crafting_table");
    registerOpaqueJsonBacked("minecraft:bookshelf");
    registerOpaqueJsonBacked("minecraft:brick_block");

    bsr.RegisterType("minecraft:furnace", {
        blockstate::BlockProperty{ "facing", { "north", "south", "west", "east" } },
        blockstate::BlockProperty{ "lit", { "false", "true" }, 0 },
    });
    const uint16_t furnaceType = bsr.GetTypeId("minecraft:furnace");
    bsr.SetDerivedData(furnaceType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:furnace");

    bsr.RegisterType("minecraft:glass", {});
    const uint16_t glassType = bsr.GetTypeId("minecraft:glass");
    bsr.SetDerivedData(glassType, [&](blockstate::BlockState& s) {
        s.isTransparent = true;
        s.renderMode = blockstate::RenderMode::Cutout;
    });
    registerJsonBackedOrLog("minecraft:glass");

    bsr.RegisterType("minecraft:torch", {
        blockstate::BlockProperty{ "facing", { "up", "east", "south", "west", "north" } },
    });
    const uint16_t torchType = bsr.GetTypeId("minecraft:torch");
    bsr.SetDerivedData(torchType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:torch");

    // === Water ===
    bsr.RegisterType("minecraft:water", {
        blockstate::BlockProperty{ "top", {"false", "true"}, 1 }
    });
    const uint16_t waterType = bsr.GetTypeId("minecraft:water");
    bsr.SetDerivedData(waterType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = true;
        s.renderMode = blockstate::RenderMode::Translucent;
        s.isFullBlock = !(bsr.GetPropertyValue(s.stateId, "top") == "true");
    });
    const auto& waterTypeDef = bsr.GetType(waterType);
    for (uint32_t i = 0; i < waterTypeDef.stateCount; ++i) {
        const uint32_t stateId = waterTypeDef.baseStateId + i;
        const bool top = (bsr.GetPropertyValue(stateId, "top") == "true");
        const blockmodels::WaterSpec waterSpec{ top };
        auto model = std::make_shared<const blockstate::BlockModel>(blockmodels::BuildWaterModel(waterSpec));
        modelLibrary.RegisterModel(stateId, std::move(model), bsr);
    }

    for (uint16_t tid = 0; tid < bsr.GetTotalTypeCount(); ++tid) {
        blockstate::AssertUniqueVariantKeysPerType(bsr, tid, "Game::InitializeBlockStates");
    }

    ASCIIgL::Logger::Info("BlockStateRegistry: " +
        std::to_string(bsr.GetTotalTypeCount()) + " types, " +
        std::to_string(bsr.GetTotalStateCount()) + " states registered.");
}

void Game::InitializeItemDefinitions() {
    auto& itemIndex = registry.ctx().emplace<ecs::data::ItemIndex>();

    using IX = ecs::data::ItemIndex;
    auto& bsr = registry.ctx().get<blockstate::BlockStateRegistry>();
    // auto blockMesh = [&](const std::string& typeName) {
    //     return IX::GetBlockMeshFromState(bsr.GetState(bsr.GetDefaultState(typeName)));
    // };

    // // === Terrain (restricted to current texture set) ===
    // itemIndex.RegisterBlockItem(registry, "minecraft:stone",       "Stone",       blockMesh("minecraft:stone"));
    // itemIndex.RegisterBlockItem(registry, "minecraft:cobblestone", "Cobblestone", blockMesh("minecraft:cobblestone"));
    // itemIndex.RegisterBlockItem(registry, "minecraft:dirt",        "Dirt",        blockMesh("minecraft:dirt"));
    // itemIndex.RegisterBlockItem(registry, "minecraft:grass",       "Grass Block", blockMesh("minecraft:grass"));
    // itemIndex.RegisterBlockItem(registry, "minecraft:bedrock",     "Bedrock",     blockMesh("minecraft:bedrock"));

    // // === Wood & Plants (subset)
    // itemIndex.RegisterBlockItem(registry, "minecraft:oak_log",       "Oak Log",       blockMesh("minecraft:oak_log"));
    // itemIndex.RegisterBlockItem(registry, "minecraft:oak_leaves",    "Oak Leaves",    blockMesh("minecraft:oak_leaves"));

    // // (Other ores, planks, utility, and special blocks are temporarily disabled
    // //  until their textures are added to the texture array.)

    // // === Resources / Materials ===
    // itemIndex.RegisterResourceItem(registry, 263, "minecraft:coal",       "Coal",       IX::GetQuadItemMesh(7, 10));
    // itemIndex.RegisterResourceItem(registry, 264, "minecraft:diamond",    "Diamond",    IX::GetQuadItemMesh(8, 10));
    // itemIndex.RegisterResourceItem(registry, 265, "minecraft:iron_ingot", "Iron Ingot", IX::GetQuadItemMesh(9, 10));
    // itemIndex.RegisterResourceItem(registry, 266, "minecraft:gold_ingot", "Gold Ingot", IX::GetQuadItemMesh(10, 10));
    // itemIndex.RegisterResourceItem(registry, 280, "minecraft:stick",      "Stick",      IX::GetQuadItemMesh(5, 9));

    // // === Swords ===
    // itemIndex.RegisterToolItem(registry, 268, "minecraft:wooden_sword",  "Wooden Sword",  IX::GetQuadItemMesh(0, 4), {2.0f, 1, 60},   {4.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 272, "minecraft:stone_sword",   "Stone Sword",   IX::GetQuadItemMesh(1, 4), {4.0f, 2, 132},  {5.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 267, "minecraft:iron_sword",    "Iron Sword",    IX::GetQuadItemMesh(2, 4), {6.0f, 3, 251},  {6.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 276, "minecraft:diamond_sword", "Diamond Sword", IX::GetQuadItemMesh(3, 4), {8.0f, 4, 1562}, {7.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 283, "minecraft:gold_sword",    "Golden Sword",  IX::GetQuadItemMesh(4, 4), {12.0f, 1, 33},  {4.0f, 1.6f});

    // // === Shovels ===
    // itemIndex.RegisterToolItem(registry, 269, "minecraft:wooden_shovel",  "Wooden Shovel",  IX::GetQuadItemMesh(0, 5), {2.0f, 1, 60},   {1.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 273, "minecraft:stone_shovel",   "Stone Shovel",   IX::GetQuadItemMesh(1, 5), {4.0f, 2, 132},  {2.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 256, "minecraft:iron_shovel",    "Iron Shovel",    IX::GetQuadItemMesh(2, 5), {6.0f, 3, 251},  {3.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 277, "minecraft:diamond_shovel", "Diamond Shovel", IX::GetQuadItemMesh(3, 5), {8.0f, 4, 1562}, {4.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 284, "minecraft:gold_shovel",    "Golden Shovel",  IX::GetQuadItemMesh(4, 5), {12.0f, 1, 33},  {1.0f, 1.6f});

    // // === Pickaxes ===
    // itemIndex.RegisterToolItem(registry, 270, "minecraft:wooden_pickaxe",  "Wooden Pickaxe",  IX::GetQuadItemMesh(0, 6), {2.0f, 1, 60},   {2.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 274, "minecraft:stone_pickaxe",   "Stone Pickaxe",   IX::GetQuadItemMesh(1, 6), {4.0f, 2, 132},  {3.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 257, "minecraft:iron_pickaxe",    "Iron Pickaxe",    IX::GetQuadItemMesh(2, 6), {6.0f, 3, 251},  {4.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 278, "minecraft:diamond_pickaxe", "Diamond Pickaxe", IX::GetQuadItemMesh(3, 6), {8.0f, 4, 1562}, {5.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 285, "minecraft:gold_pickaxe",    "Golden Pickaxe",  IX::GetQuadItemMesh(4, 6), {12.0f, 1, 33},  {2.0f, 1.6f});

    // // === Axes ===
    // itemIndex.RegisterToolItem(registry, 271, "minecraft:wooden_axe",  "Wooden Axe",  IX::GetQuadItemMesh(0, 7), {2.0f, 1, 60},   {3.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 275, "minecraft:stone_axe",   "Stone Axe",   IX::GetQuadItemMesh(1, 7), {4.0f, 2, 132},  {4.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 258, "minecraft:iron_axe",    "Iron Axe",    IX::GetQuadItemMesh(2, 7), {6.0f, 3, 251},  {5.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 279, "minecraft:diamond_axe", "Diamond Axe", IX::GetQuadItemMesh(3, 7), {8.0f, 4, 1562}, {6.0f, 1.6f});
    // itemIndex.RegisterToolItem(registry, 286, "minecraft:gold_axe",    "Golden Axe",  IX::GetQuadItemMesh(4, 7), {12.0f, 1, 33},  {3.0f, 1.6f});
}
