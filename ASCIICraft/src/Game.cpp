#include <ASCIICraft/game/Game.hpp>

#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/RendererGPU.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/gui/GuiManager.hpp>

Game::Game() 
    : gameState(GameState::Playing)
    , isRunning(false) {
}

Game::~Game() {
    Shutdown();
}

bool Game::Initialize() {
    Logger::Info("Initializing ASCIICraft...");

    Logger::Info("Setting up palette and screen...");

    std::array<PaletteEntry, 15> paletteEntries = {{
        { {3, 0, 0}, 0x1 },        // #330701
        { {4, 0, 0}, 0x2 },        // #490902
        { {5, 0, 0}, 0x3 },        // #5E0C03
        { {7, 0, 0}, 0x4 },        // #730E03
        { {8, 1, 0}, 0x5 },        // #881104
        { {9, 1, 0}, 0x6 },        // #9D1405
        { {11, 1, 0}, 0x7 },       // #B31705
        { {12, 1, 0}, 0x8 },       // #C51906
        { {13, 1, 0}, 0x9 },       // #D81B06
        { {15, 1, 0}, 0xA },       // #F01E08
        { {15, 2, 1}, 0xB },       // #F72811
        { {15, 3, 2}, 0xC },       // #F73B26
        { {15, 4, 3}, 0xD },       // #F84734
        { {15, 6, 5}, 0xE },       // #F96352
        { {15, 7, 7}, 0xF }        // #FA7F70
    }};

    Palette gamePalette = Palette(paletteEntries); // Custom palette with black auto-added at 0

    // Initialize screen
    if (Screen::GetInst().Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, L"ASCIICraft", FONT_SIZE, gamePalette) != 0) {
        Logger::Error("Failed to initialize screen");
        return false;
    }
    SCREEN_WIDTH = Screen::GetInst().GetWidth();
    SCREEN_HEIGHT = Screen::GetInst().GetHeight();

    FPSClock::GetInst().Initialize(static_cast<unsigned int>(TARGET_FPS), 1.0f);

    Renderer::GetInst().SetBackgroundCol(gamePalette.GetRGB(0));
    
    Renderer::GetInst().SetWireframe(false);
    Renderer::GetInst().SetBackfaceCulling(true);
    Renderer::GetInst().SetCCW(true);
    Renderer::GetInst().SetDiagnosticsEnabled(true);

    Renderer::GetInst().Initialize(true, 4, false); // Enable antialiasing with 4 samples, not CPU only
    
    // Load resources
    if (!LoadResources()) {
        Logger::Error("Failed to load resources");
        return false;
    }
    
    // Initialize game systems
    world = std::make_unique<World>(32, WorldPos(0, 90, 0), 32);
    inputManager = std::make_unique<InputManager>();
    
    // Create player at spawn point
    player = std::make_unique<Player>(world->GetSpawnPoint().ToVec3(), GameMode::Spectator);
    
    // Connect player to world
    world->SetPlayer(player.get());
    world->GenerateWorld();

    gameState = GameState::Playing;
    isRunning = true;
    
    Logger::Info("ASCIICraft initialized successfully!");
    return true;
}

void Game::Run() {
    if (!Initialize()) {
        Logger::Error("Failed to initialize game");
        return;
    }
    
    Logger::Info("Starting game loop...");

    Profiler::GetInst().SetEnabled(true);

    // Main game loop
    int frameCounter = 0;
    while (isRunning) {
        Profiler::GetInst().BeginFrame();
        FPSClock::GetInst().StartFPSClock();
        
        {
            PROFILE_SCOPE("HandleInput");
            HandleInput();
        }
        
        {
            PROFILE_SCOPE("Update");
            Update();
        }
        
        {
            PROFILE_SCOPE("RenderGame");
            Render();
        }
        
        FPSClock::GetInst().EndFPSClock();
        
        // profiling work
        Profiler::GetInst().EndFrame();
        frameCounter++;
        if (frameCounter % 60 == 0) { 
            frameCounter = 0;
            Logger::Info("FPS: " + std::to_string(FPSClock::GetInst().GetFPS()));
            Profiler::GetInst().LogReport();
            Profiler::GetInst().Reset();  // Reset profiler data after logging
        }
    }
    
    Shutdown();
}

void Game::Update() {
    switch (gameState) {
        case GameState::Playing:
            // Update player
            if (player) {
                player->Update(world.get());
            }
            
            // Update world (chunk loading, physics, etc.)
            world->Update();
            break;
        case GameState::Exiting:
            isRunning = false;
            break;
    }
}

void Game::Render() {    
    {
        PROFILE_SCOPE("Clear Px Buff/Begin Frame");
        Screen::GetInst().ClearPixelBuffer();
        Renderer::GetInst().BeginColBuffFrame();
    }

    switch (gameState) {
        case GameState::Playing:
            {
                PROFILE_SCOPE("Render.RenderPlaying");
                RenderPlaying();
            }
            break;
    }

    {
        PROFILE_SCOPE("Render.EndColBuffFrame");
        Renderer::GetInst().EndColBuffFrame();  // Present for RenderDoc
        
    }
    // pixel buffer draws
    {
        PROFILE_SCOPE("Render.PixelBufferDraws");
        Renderer::GetInst().DrawScreenBorderPxBuff(0xF);
    }
    {
        PROFILE_SCOPE("Render.PixelBufferOutput");
        Screen::GetInst().OutputBuffer();
    }
    
}

void Game::HandleInput() {
    inputManager->Update();
    
    // Process player input
    if (player && gameState == GameState::Playing) {
        player->HandleInput(*inputManager);
    }
    
    // Handle exit input
    if (inputManager->IsActionPressed("quit")) {
        gameState = GameState::Exiting;
    }
}

void Game::Shutdown() {
    Logger::Info("Shutting down ASCIICraft...");
    
    // Clear the block atlas reference before destroying it
    Block::SetTextureAtlas(nullptr);
    
    // Clean up resources
    player.reset();
    world.reset();
    inputManager.reset();
    blockAtlas.reset();  // Destroy the texture atlas
    
    // Screen cleanup happens automatically in destructor
    Logger::Info("ASCIICraft shutdown complete");
}

bool Game::LoadResources() {
    Logger::Info("Loading game resources...");
    
    // Load block texture atlas - store as member to prevent destruction
    blockAtlas = std::make_unique<Texture>("res/textures/terrain.png");
    if (!blockAtlas) {
        Logger::Error("Failed to load block texture atlas");
        return false;
    }
    
    // Set the global block atlas
    Block::SetTextureAtlas(blockAtlas.get());
    
    Logger::Info("Resources loaded successfully");
    return true;
}

void Game::RenderPlaying() {
    world->Render();
}