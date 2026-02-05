#include <ASCIICraft/game/Game.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/rendering/TerrainShaders.hpp>

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

// ecs components
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>

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
    ASCIIgL::Palette gamePalette = frostCyanPalette;

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
    auto blockTextureArray = ASCIIgL::TextureLibrary::GetInst().LoadTextureArray("res/textures/terrain.png", 16);
    
    if (!blockTextureArray || !blockTextureArray->IsValid()) {
        ASCIIgL::Logger::Error("Failed to load block texture array");
        return false;
    }

    // Set static pointer for block system
    Block::SetTextureArray(blockTextureArray.get());
    
    // Create gradient mapping shader program
    auto vs = ASCIIgL::Shader::CreateFromSource(
        TerrainShaders::GetTerrainVSSource(),
        ASCIIgL::ShaderType::Vertex
    );
    
    auto ps = ASCIIgL::Shader::CreateFromSource(
        TerrainShaders::GetTerrainPSSource(),
        ASCIIgL::ShaderType::Pixel
    );
    
    if (!vs || !vs->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile gradient map vertex shader: " + vs->GetCompileError());
        return false;
    }
    
    if (!ps || !ps->IsValid()) {
        ASCIIgL::Logger::Error("Failed to compile gradient map pixel shader: " + ps->GetCompileError());
        return false;
    }
    
    // Create shader program with gradient map uniform layout
    auto blockShaderProgram = ASCIIgL::ShaderProgram::Create(
        std::move(vs),
        std::move(ps),
        ASCIIgL::VertFormats::PosUVLayer(),
        TerrainShaders::GetTerrainPSUniformLayout()
    );
    
    if (!blockShaderProgram || !blockShaderProgram->IsValid()) {
        ASCIIgL::Logger::Error("Failed to create gradient map shader program");
        return false;
    }

    // Create material and register it
    std::shared_ptr<ASCIIgL::ShaderProgram> sharedProgram = std::move(blockShaderProgram);
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