#include <ASCIICraft/game/Game.hpp>
#include <ASCIICraft/world/World.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/MaterialBuilder.hpp>

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
#include <ASCIICraft/textures/BlockTextureCatalog.hpp>
#include <ASCIICraft/textures/ItemTextureCatalog.hpp>
#include <ASCIICraft/textures/TextureCatalog.hpp>
#include <ASCIICraft/world/block/models/WaterModelBuilder.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

#include <ASCIICraft/gui/screens/PlayHUDScreen.hpp>
#include <ASCIICraft/gui/screens/InventoryScreen.hpp>
#include <ASCIICraft/gui/text/BitmapFont.hpp>

// ecs components
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/sound/SoundRegistry.hpp>

// shaders
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIICraft/rendering/TerrainShaders.hpp>
#include <ASCIICraft/rendering/DroppedItemShaders.hpp>
#include <ASCIICraft/rendering/BlockTargetOutlineShaders.hpp>

Game::Game()
    : gameState(GameState::Playing)
    , inputSystem(eventBus)
    , gameplayInputFilter(inputSystem)
    , movementSystem(registry, gameplayInputFilter, eventBus)
    , physicsSystem(registry)
    , blockTargetSystem(registry)
    , ecsRenderSystem(registry)
    , cameraSystem(registry, gameplayInputFilter)
    , guiManager(registry, eventBus, inputSystem)
    , blockUpdateSystem(registry, eventBus)
    , placingSystem(registry, eventBus)
    , miningSystem(registry, eventBus)
    , inventorySystem(registry, eventBus)
    , droppedItemSystem(registry, eventBus)
    , hotbarSystem(registry)
    , lifetimeSystem(registry)
    , particleSystem(registry, eventBus)
    , soundSystem(registry, eventBus)
    , musicSystem(eventBus, soundSystem)
    , stepSfxSystem(registry, eventBus)
    , playerFactory(registry)
    , shouldInternalExit(false)
{
    ASCIIgL::Logger::Debug("Game constructor: systems created, registry bound.");
}

Game::~Game() {
    ASCIIgL::Logger::Debug("Game destructor called.");
    Shutdown();
}

bool Game::Initialize(bool renderToTerminal, bool multicolor) {
    ASCIIgL::Logger::Info("Initializing ASCIICraft...");

    ASCIIgL::Logger::Debug("Preloading textures for palette generation...");

    LoadTextures(multicolor);

    std::vector<std::pair<float, std::shared_ptr<ASCIIgL::Texture>>> textureWeights;
    std::vector<std::pair<float, std::shared_ptr<ASCIIgL::TextureArray>>> textureArrayWeights = {
        {1.0f, ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray")},
        {0.0f, ASCIIgL::TextureLibrary::GetInst().GetTextureArray("itemTextureArray")}
    };

    std::unique_ptr<ASCIIgL::Palette> gamePalette = multicolor
        ? std::make_unique<ASCIIgL::Palette>(textureWeights, textureArrayWeights)
        : std::make_unique<ASCIIgL::MonochromePalette>(textureWeights, textureArrayWeights);

    ASCIIgL::Logger::Debug("Initializing screen...");
    if (ASCIIgL::Screen::GetInst().Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, L"ASCIICraft", FONT_SIZE, *gamePalette, renderToTerminal) != 0) {
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
    renderer.SetDitheringEnabled(false);
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

    ASCIIgL::Logger::Debug("Initializing item definitions...");
    InitializeItemDefinitions();

    ASCIIgL::Logger::Debug("Initializing player...");
    InitializePlayer();

    ASCIIgL::Logger::Debug("Initializing ECS systems...");
    InitializeSystems();

    ASCIIgL::Logger::Debug("Loading resources...");
    if (!LoadResources()) {
        ASCIIgL::Logger::Error("Failed to load resources");
        return false;
    }

    if (!LoadFont()) {
        ASCIIgL::Logger::Error("Failed to load GUI font");
        return false;
    }

    ASCIIgL::Logger::Debug("Initializing GUI...");
    InitializeGUI();

    ASCIIgL::InputManager::GetInst().Initialize();
    ASCIIgL::Logger::Debug("InputManager initialized.");

    gameState = GameState::Playing;

    ASCIIgL::Logger::Info("ASCIICraft initialized successfully!");
    return true;
}

void Game::Run(std::function<bool()> shouldExternalExit, bool renderToTerminal, bool multicolor) {
    if (!Initialize(renderToTerminal, multicolor)) {
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

    if (ASCIIgL::InputManager::GetInst().IsKeyDown(ASCIIgL::Key::K)) {
        ASCIIgL::Renderer::GetInst().SetDitheringEnabled(!ASCIIgL::Renderer::GetInst().GetDitheringEnabled());
    }

    for ([[maybe_unused]] const auto& e : eventBus.view<events::ToggleInventoryEvent>()) {
        if (!inventoryScreen_) continue;

        // Minecraft-like: hotbar (base HUD) stays visible; inventory overlays on top.
        guiManager.ToggleScreen(inventoryScreen_.get());
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

            hotbarSystem.Update();

            movementSystem.Update();
            cameraSystem.Update();
            physicsSystem.Update();

            blockTargetSystem.SetGameplayActive(!guiBlocking);
            blockTargetSystem.Update();

            miningSystem.Update();
            placingSystem.Update();
            blockUpdateSystem.Update();

            particleSystem.Update();

            droppedItemSystem.Update();
            inventorySystem.Update();

            stepSfxSystem.Update();
            musicSystem.Update();
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

bool Game::LoadTextures(bool multicolor) {
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
    monoMap.enabled = !multicolor;
    monoMap.darkL = darkL;
    monoMap.lightL = lightL;
    monoMap.hueDir = NeutralGrayHue;
    monoMap.brightness = 1.0f;
    monoMap.contrast = 1.0f;

    // Load block textures from central catalog order.
    std::vector<std::string> blockTexturePaths =
        textures::BuildTexturePaths(blocktextures::GetBlockTextureCatalog());

    auto blockTextureArray = ASCIIgL::TextureLibrary::GetInst().LoadTextureArray(blockTexturePaths, "terrainTextureArray", monoMap);
    if (!blockTextureArray || !blockTextureArray->IsValid()) {
        ASCIIgL::Logger::Error("Failed to load block texture array");
        return false;   
    }

    std::vector<std::string> itemTexturePaths =
        textures::BuildTexturePaths(itemtextures::GetItemTextureCatalog());
    auto itemTextureArray = ASCIIgL::TextureLibrary::GetInst().LoadTextureArray(itemTexturePaths, "itemTextureArray", monoMap);
    if (!itemTextureArray || !itemTextureArray->IsValid()) {
        ASCIIgL::Logger::Error("Failed to load item texture array");
        return false;
    }

    auto inventoryTexture = ASCIIgL::TextureLibrary::GetInst().LoadTexture(
        "res/textures/gui/container/inventory.png", "inventoryTexture", ASCIIgL::MonochromeMapping{}
    );
    if (!inventoryTexture) {
        ASCIIgL::Logger::Error("Failed to load inventory texture");
        return false;
    }

    auto widgetsTexture = ASCIIgL::TextureLibrary::GetInst().LoadTexture(
        "res/textures/gui/widgets.png", "widgetsTexture", ASCIIgL::MonochromeMapping{}
    );
    if (!widgetsTexture) {
        ASCIIgL::Logger::Error("Failed to load widgets texture");
        return false;
    }

    auto cursorTexture = ASCIIgL::TextureLibrary::GetInst().LoadTexture(
        "res/textures/gui/cursor.png", "cursorTexture", ASCIIgL::MonochromeMapping{}
    );
    if (!cursorTexture) {
        ASCIIgL::Logger::Error("Failed to load cursor texture");
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
    if (!LoadDroppedItemMaterial())  return false;
    if (!LoadGUIMaterial())          return false;
    if (!LoadGUIItemMaterial())      return false;
    if (!LoadGUIBlockMaterial())     return false;
    if (!LoadGUITextMaterial())           return false;
    if (!LoadBlockTargetOutlineMaterial()) return false;

    ASCIIgL::Logger::Info("Resources loaded successfully");
    return true;
}

bool Game::LoadTerrainMaterial() {
    return ASCIIgL::BuildAndRegisterMaterial({
        "blockMaterial",
        TerrainShaders::GetTerrainVSSource(),
        TerrainShaders::GetTerrainPSSource(),
        ASCIIgL::VertFormats::PosUVLayer(),
        TerrainShaders::GetTerrainPSUniformLayout(),
        true,
        false,
        [](ASCIIgL::Material& material) {
            auto terrainTextureArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
            if (!terrainTextureArray) {
                ASCIIgL::Logger::Error("terrainTextureArray missing for terrain material");
                return false;
            }
            material.SetTextureArray(0, terrainTextureArray.get());
            return true;
        }
    });
}

bool Game::LoadDroppedItemMaterial() {
    if (!ASCIIgL::BuildAndRegisterMaterial({
        "droppedItemBlockMaterial",
        DroppedItemShaders::GetVSSource(),
        DroppedItemShaders::GetPSSource(),
        ASCIIgL::VertFormats::PosUVLayer(),
        DroppedItemShaders::GetUniformLayout(),
        true,
        true,
        [](ASCIIgL::Material& material) {
            auto terrainTextureArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
            if (!terrainTextureArray) {
                ASCIIgL::Logger::Error("terrainTextureArray missing for dropped item block material");
                return false;
            }
            material.SetTextureArray(0, terrainTextureArray.get());
            return true;
        }
    })) {
        return false;
    }

    auto iconMaterial = ASCIIgL::MaterialLibrary::GetInst().GetOrCreateFromTemplate(
        "droppedItemBlockMaterial", "droppedItemMaterial");
    if (!iconMaterial) {
        ASCIIgL::Logger::Error("Failed to create droppedItemMaterial from template");
        return false;
    }

    auto itemTextureArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("itemTextureArray");
    if (!itemTextureArray) {
        ASCIIgL::Logger::Error("itemTextureArray missing for dropped item material");
        return false;
    }
    iconMaterial->SetTextureArray(0, itemTextureArray.get());
    return true;
}

bool Game::LoadGUIMaterial() {
    return ASCIIgL::BuildAndRegisterMaterial({
        "guiMaterial",
        ASCIIgL::DefaultShaders::GetDefaultVertexShaderSource(),
        ASCIIgL::DefaultShaders::GetDefaultPixelShaderSource(),
        ASCIIgL::VertFormats::PosUV(),
        ASCIIgL::DefaultShaders::GetDefaultUniformLayout(),
        false,
        false
    });
}

bool Game::LoadGUIItemMaterial() {
    return ASCIIgL::BuildAndRegisterMaterial({
        "guiItemMaterial",
        ASCIIgL::DefaultShaders::GetTextureArrayVertexShaderSource(),
        ASCIIgL::DefaultShaders::GetTextureArrayPixelShaderSource(),
        ASCIIgL::VertFormats::PosUVLayer(),
        ASCIIgL::DefaultShaders::GetDefaultUniformLayout(),
        false,
        false,
        [](ASCIIgL::Material& material) {
            auto itemTextureArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("itemTextureArray");
            if (!itemTextureArray) {
                ASCIIgL::Logger::Error("itemTextureArray missing for GUI item material");
                return false;
            }
            material.SetTextureArray(0, itemTextureArray.get());
            return true;
        }
    });
}

bool Game::LoadGUIBlockMaterial() {
    return ASCIIgL::BuildAndRegisterMaterial({
        "guiBlockMaterial",
        ASCIIgL::DefaultShaders::GetTextureArrayVertexShaderSource(),
        ASCIIgL::DefaultShaders::GetTextureArrayPixelShaderSource(),
        ASCIIgL::VertFormats::PosUVLayer(),
        ASCIIgL::DefaultShaders::GetDefaultUniformLayout(),
        false,
        false,
        [](ASCIIgL::Material& material) {
            auto terrainTextureArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
            if (!terrainTextureArray) {
                ASCIIgL::Logger::Error("terrainTextureArray missing for GUI block material");
                return false;
            }
            material.SetTextureArray(0, terrainTextureArray.get());
            return true;
        }
    });
}

bool Game::LoadGUITextMaterial() {
    return ASCIIgL::BuildAndRegisterMaterial({
        "guiTextMaterial",
        ASCIIgL::DefaultShaders::GetTextureArrayVertexShaderSource(),
        ASCIIgL::DefaultShaders::GetTextureArrayPixelShaderSource(),
        ASCIIgL::VertFormats::PosUVLayer(),
        ASCIIgL::DefaultShaders::GetDefaultUniformLayout(),
        false,
        false,
        [](ASCIIgL::Material& material) {
            auto fontTextureArray = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("defaultFontTextureArray");
            if (!fontTextureArray) {
                ASCIIgL::Logger::Error("defaultFontTextureArray missing for GUI text material");
                return false;
            }
            material.SetTextureArray(0, fontTextureArray.get());
            return true;
        }
    });
}

bool Game::LoadBlockTargetOutlineMaterial() {
    return ASCIIgL::BuildAndRegisterMaterial({
        "blockTargetOutlineMaterial",
        BlockTargetOutlineShaders::GetVSSource(),
        BlockTargetOutlineShaders::GetPSSource(),
        ASCIIgL::VertFormats::PosColor(),
        BlockTargetOutlineShaders::GetUniformLayout(),
        false,
        false
    });
}

bool Game::LoadFont() {
    constexpr int kDefaultFontStartingLayer = 33;
    constexpr const char* kDefaultFontCharacters =
        R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)";

    auto guiFont = gui::text::LoadBitmapFont(
        ASCIIgL::TextureLibrary::GetInst().GetTextureArray("defaultFontTextureArray"),
        kDefaultFontStartingLayer,
        kDefaultFontCharacters
    );
    if (!guiFont) {
        ASCIIgL::Logger::Error("Failed to load default GUI bitmap font");
        return false;
    }

    registry.ctx().emplace<std::shared_ptr<gui::text::BitmapFont>>(std::move(guiFont));
    return true;
}

void Game::RenderPlaying() {
    // Keep 2D GUI camera in sync with viewport (GPU pipeline uses Screen dimensions; 2D ortho must match)
    GetWorldPtr(registry)->Render();
    // blockTargetSystem.Render(); // Outline rendering disabled for now.
    ecsRenderSystem.Render();
    guiManager.Render();
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

    auto& soundRegistry = registry.ctx().emplace<sound::SoundRegistry>();
    sound::RegisterDefaultSounds(soundRegistry);

    ASCIIgL::Logger::Debug("Systems initialized.");
}

void Game::InitializeGUI() {
    ASCIIgL::Logger::Debug("Initializing GUI screens...");
    guiManager.BuildCursorSurface();

    const entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player == entt::null) {
        ASCIIgL::Logger::Warning("PlayHUDScreen not created: player entity missing.");
        return;
    }

    playHudScreen_ = std::make_unique<gui::PlayHUDScreen>(
        registry, eventBus, player, guiManager.GetMeshLibrary()
    );
    guiManager.SetBaseScreen(playHudScreen_.get());
    playHudScreen_->Layout(
        {static_cast<float>(ASCIIgL::Screen::GetInst().GetWidth()),
         static_cast<float>(ASCIIgL::Screen::GetInst().GetHeight())},
        nullptr
    );
    ASCIIgL::Logger::Debug("GUI initialized: PlayHUDScreen set as base screen.");

    // Create inventory screen but do not push it until E is pressed.
    inventoryScreen_ = std::make_unique<gui::InventoryScreen>(
        registry, eventBus, player, guiManager.GetMeshLibrary()
    );
    inventoryScreen_->Layout(
        {static_cast<float>(ASCIIgL::Screen::GetInst().GetWidth()),
         static_cast<float>(ASCIIgL::Screen::GetInst().GetHeight())},
        nullptr
    );
    ASCIIgL::Logger::Debug("GUI initialized: InventoryScreen created (not pushed).");
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
    auto& itemRegistry = registry.ctx().emplace<ecs::data::ItemRegistry>();

    const auto itemLayer = [](const char* textureId) -> float {
        return static_cast<float>(
            textures::GetLayerForTextureId(itemtextures::GetItemTextureCatalog(), textureId)
        );
    };

    // === Block items (all registered block types except air and water) ===
    itemRegistry.RegisterBlockItem(registry, "minecraft:bedrock",          "Bedrock");
    itemRegistry.RegisterBlockItem(registry, "minecraft:stone",            "Stone");
    itemRegistry.RegisterBlockItem(registry, "minecraft:dandelion",        "Dandelion");
    itemRegistry.RegisterBlockItem(registry, "minecraft:poppy",            "Poppy");
    itemRegistry.RegisterBlockItem(registry, "minecraft:tall_grass",       "Tall Grass");
    itemRegistry.RegisterBlockItem(registry, "minecraft:fern",             "Fern");
    itemRegistry.RegisterBlockItem(registry, "minecraft:fence",            "Oak Fence");
    itemRegistry.RegisterBlockItem(registry, "minecraft:cobblestone",      "Cobblestone");
    itemRegistry.RegisterBlockItem(registry, "minecraft:stone_stairs",     "Stone Stairs");
    itemRegistry.RegisterBlockItem(registry, "minecraft:cobblestone_slab", "Cobblestone Slab");
    itemRegistry.RegisterBlockItem(registry, "minecraft:dirt",             "Dirt");
    itemRegistry.RegisterBlockItem(registry, "minecraft:grass",            "Grass Block");
    itemRegistry.RegisterBlockItem(registry, "minecraft:oak_log",          "Oak Log");
    itemRegistry.RegisterBlockItem(registry, "minecraft:oak_planks",         "Oak Planks");
    itemRegistry.RegisterBlockItem(registry, "minecraft:oak_slab",         "Oak Slab");
    itemRegistry.RegisterBlockItem(registry, "minecraft:oak_leaves",       "Oak Leaves");
    itemRegistry.RegisterBlockItem(registry, "minecraft:crafting_table",   "Crafting Table");
    itemRegistry.RegisterBlockItem(registry, "minecraft:bookshelf",        "Bookshelf");
    itemRegistry.RegisterBlockItem(registry, "minecraft:brick_block",      "Bricks");
    itemRegistry.RegisterBlockItem(registry, "minecraft:furnace",          "Furnace");
    itemRegistry.RegisterBlockItem(registry, "minecraft:glass",            "Glass");
    itemRegistry.RegisterBlockItem(
        registry,
        "minecraft:torch",
        "Torch",
        64,
        ecs::components::ItemGuiMeshTransform::DefaultTorch());

    // === Resources / Materials ===
    itemRegistry.RegisterResourceItem(registry, "minecraft:coal",       "Coal",       itemLayer("minecraft:items/coal"));
    itemRegistry.RegisterResourceItem(registry, "minecraft:iron_ingot", "Iron Ingot", itemLayer("minecraft:items/iron_ingot"));
    itemRegistry.RegisterResourceItem(registry, "minecraft:gold_ingot", "Gold Ingot", itemLayer("minecraft:items/gold_ingot"));
    itemRegistry.RegisterResourceItem(registry, "minecraft:stick",      "Stick",      itemLayer("minecraft:items/stick"));

    // === Swords ===
    itemRegistry.RegisterToolItem(registry, "minecraft:wooden_sword",  "Wooden Sword",  itemLayer("minecraft:items/wood_sword"),  {2.0f, 1, 60},   {4.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:stone_sword",   "Stone Sword",   itemLayer("minecraft:items/stone_sword"), {4.0f, 2, 132},  {5.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:iron_sword",    "Iron Sword",    itemLayer("minecraft:items/iron_sword"),  {6.0f, 3, 251},  {6.0f, 1.6f});
    // === Shovels ===
    itemRegistry.RegisterToolItem(registry, "minecraft:wooden_shovel",  "Wooden Shovel",  itemLayer("minecraft:items/wood_shovel"),  {2.0f, 1, 60},   {1.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:stone_shovel",   "Stone Shovel",   itemLayer("minecraft:items/stone_shovel"), {4.0f, 2, 132},  {2.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:iron_shovel",    "Iron Shovel",    itemLayer("minecraft:items/iron_shovel"),  {6.0f, 3, 251},  {3.0f, 1.6f});
    // === Pickaxes ===
    itemRegistry.RegisterToolItem(registry, "minecraft:wooden_pickaxe",  "Wooden Pickaxe",  itemLayer("minecraft:items/wood_pickaxe"),  {2.0f, 1, 60},   {2.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:stone_pickaxe",   "Stone Pickaxe",   itemLayer("minecraft:items/stone_pickaxe"), {4.0f, 2, 132},  {3.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:iron_pickaxe",    "Iron Pickaxe",    itemLayer("minecraft:items/iron_pickaxe"),  {6.0f, 3, 251},  {4.0f, 1.6f});

    // === Axes ===
    itemRegistry.RegisterToolItem(registry, "minecraft:wooden_axe",  "Wooden Axe",  itemLayer("minecraft:items/wood_axe"),  {2.0f, 1, 60},   {3.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:stone_axe",   "Stone Axe",   itemLayer("minecraft:items/stone_axe"), {4.0f, 2, 132},  {4.0f, 1.6f});
    itemRegistry.RegisterToolItem(registry, "minecraft:iron_axe",    "Iron Axe",    itemLayer("minecraft:items/iron_axe"),  {6.0f, 3, 251},  {5.0f, 1.6f});
}
