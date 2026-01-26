#include <ASCIICraft/game/Game.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Palette.hpp>

#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>
#include <ASCIIgL/renderer/gpu/Material.hpp>

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

    // ASCIIgL initializations
    std::array<ASCIIgL::PaletteEntry, 16> paletteEntries = {{
        {{0, 0, 0}, 0x0},
        {{3, 0, 0}, 0x1},
        {{4, 0, 0}, 0x2},
        {{5, 0, 0}, 0x3},
        {{7, 0, 0}, 0x4},
        {{8, 1, 0}, 0x5},
        {{9, 1, 0}, 0x6},
        {{11, 1, 0}, 0x7},
        {{12, 1, 0}, 0x8},
        {{13, 1, 0}, 0x9},
        {{15, 1, 0}, 0xA},
        {{15, 2, 1}, 0xB},
        {{15, 3, 2}, 0xC},
        {{15, 4, 3}, 0xD},
        {{15, 6, 5}, 0xE},
        {{15, 7, 7}, 0xF},
    }};

    ASCIIgL::Palette gamePalette = ASCIIgL::Palette(paletteEntries);

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

    ASCIIgL::Renderer::GetInst().SetBackgroundCol(gamePalette.GetRGB(1));
    ASCIIgL::Renderer::GetInst().SetWireframe(false);
    ASCIIgL::Renderer::GetInst().SetBackfaceCulling(true);
    ASCIIgL::Renderer::GetInst().SetCCW(true);
    ASCIIgL::Renderer::GetInst().SetDiagnosticsEnabled(true);

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
            ASCIIgL::PROFILE_SCOPE("HandleInput");
            HandleInput();
        }

        {
            ASCIIgL::PROFILE_SCOPE("Update");
            Update();
        }

        {
            ASCIIgL::PROFILE_SCOPE("RenderGame");
            Render();
        }

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

    switch (gameState) {
        case GameState::Playing: {

            ASCIIgL::Logger::Debug("Update: Step 1 - Retrieving world pointer...");
            World* world = GetWorldPtr(registry);
            if (!world) {
                ASCIIgL::Logger::Error("Update: World pointer is NULL. Aborting update.");
                return;
            }

            ASCIIgL::Logger::Debug("Update: Step 2 - Calling world->Update()...");
            world->Update();
            ASCIIgL::Logger::Debug("Update: world->Update() completed.");

            ASCIIgL::Logger::Debug("Update: Step 3 - movementSystem.Update()...");
            movementSystem.Update();
            ASCIIgL::Logger::Debug("Update: movementSystem.Update() completed.");

            ASCIIgL::Logger::Debug("Update: Step 4 - cameraSystem.Update()...");
            cameraSystem.Update();
            ASCIIgL::Logger::Debug("Update: cameraSystem.Update() completed.");

            ASCIIgL::Logger::Debug("Update: Step 5 - physicsSystem.Update()...");
            physicsSystem.Update();
            ASCIIgL::Logger::Debug("Update: physicsSystem.Update() completed.");

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

void Game::HandleInput() {
    ASCIIgL::InputManager::GetInst().Update();

    if (ASCIIgL::InputManager::GetInst().IsActionPressed("quit")) {
        ASCIIgL::Logger::Info("Quit action detected. Exiting game...");
        gameState = GameState::Exiting;
    }
}

void Game::Shutdown() {
    ASCIIgL::Logger::Info("Shutting down ASCIICraft...");

    Block::SetTextureAtlas(nullptr);
    blockAtlas.reset();

    ASCIIgL::Logger::Info("ASCIICraft shutdown complete");
}

bool Game::LoadResources() {
    ASCIIgL::Logger::Info("Loading game resources...");

    blockAtlas = std::make_unique<ASCIIgL::Texture>("res/textures/terrain.png");
    if (!blockAtlas) {
        ASCIIgL::Logger::Error("Failed to load block texture atlas");
        return false;
    }

    Block::SetTextureAtlas(blockAtlas.get());

    ASCIIgL::Logger::Info("Resources loaded successfully");
    return true;
}

void Game::RenderPlaying() {
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

    auto &pm = registry.ctx().emplace<ecs::managers::PlayerManager>(registry);
    ASCIIgL::Logger::Debug("PlayerManager created.");

    pm.createPlayerEnt(GetWorldPtr(registry)->GetSpawnPoint().ToVec3(), GameMode::Survival);
    ASCIIgL::Logger::Debug("Player entity created");
}

void Game::InitializeSystems() {
    ASCIIgL::Logger::Debug("Initializing render system camera...");

    auto *playerManager = ecs::managers::GetPlayerPtr(registry);
    renderSystem.SetActive3DCamera(registry.try_get<ecs::components::PlayerCamera>(playerManager->getPlayerEnt()));

    ASCIIgL::Logger::Debug("Systems initialized.");
}