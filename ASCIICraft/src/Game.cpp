#include <ASCIICraft/game/Game.hpp>

#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Palette.hpp>

#include <ASCIIgL/engine/Logger.hpp>
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
        { {15, 15, 15}, 0x1 },        // White (#FFFFFF)
        { {3, 4, 3}, 0x2 },           // Dark Gray Green (#323C39)
        { {6, 6, 6}, 0x3 },           // Medium Gray (#696A6A)
        { {8, 7, 8}, 0x4 },           // Light Gray Purple (#847E87)
        { {6, 3, 3}, 0x5 },           // Dark Brown Red (#663931)
        { {8, 5, 3}, 0x6 },           // Medium Brown (#8F563B)
        { {13, 10, 6}, 0x7 },         // Light Brown (#D9A066)
        { {8, 6, 3}, 0x8 },           // Dark Yellow Brown (#8A6F30)
        { {8, 9, 4}, 0x9 },           // Olive Green (#8F974A)
        { {15, 15, 3}, 0xA },         // Bright Yellow (#FBF236)
        { {4, 6, 2}, 0xB },           // Dark Forest Green (#4B692F)
        { {6, 11, 3}, 0xC },          // Medium Green (#6ABE30)
        { {9, 14, 5}, 0xD },          // Bright Green (#99E550)
        { {5, 12, 14}, 0xE },         // Light Blue (#5FCDE4)
        { {9, 10, 11}, 0xF }          // Light Gray Blue (#9BADB7)
    }};
    Palette gamePalette = Palette(paletteEntries); // Custom palette with black auto-added at 0

    // Initialize screen
    if (Screen::GetInst().Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, L"ASCIICraft", FONT_SIZE, gamePalette) != SCREEN_NOERROR) {
        Logger::Error("Failed to initialize screen");
        return false;
    }
    SCREEN_WIDTH = Screen::GetInst().GetWidth();
    SCREEN_HEIGHT = Screen::GetInst().GetHeight();

    FPSClock::GetInst().Initialize(static_cast<unsigned int>(TARGET_FPS), 1.0f);

    Renderer::GetInst().Initialize();
    Renderer::GetInst().SetBackgroundCol(gamePalette.GetRGB(14));
    
    Renderer::GetInst().SetWireframe(false);
    Renderer::GetInst().SetBackfaceCulling(true);
    Renderer::GetInst().SetCCW(true);
    Renderer::GetInst().SetContrast(1.1f);
	Renderer::GetInst().SetAntialiasingsamples(4);
	Renderer::GetInst().SetAntialiasing(true);
    Renderer::GetInst().SetDiagnosticsEnabled(false);

    // Load resources
    if (!LoadResources()) {
        Logger::Error("Failed to load resources");
        return false;
    }
    
    // Initialize game systems
    world = std::make_unique<World>(2, WorldPos(0, 5, 0), 2);
    inputManager = std::make_unique<InputManager>();
    
    // Create player at spawn point
    player = std::make_unique<Player>(world->GetSpawnPoint().ToVec3());
    
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

    // Main game loop
    int frameCounter = 0;
    while (isRunning) {
        Profiler::GetInst().BeginFrame();

        FPSClock::GetInst().StartFPSClock();
        
        {
            PROFILE_SCOPE("ClearBuffers");
            Screen::GetInst().ClearPixelBuffer();
            Renderer::GetInst().ClearBuffers();
        }
        
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

        // color buffer draws ending
        {
            PROFILE_SCOPE("ColorBufferOverwrite");
            Renderer::GetInst().OverwritePxBuffWithColBuff();
        }
        
        // pixel buffer draws
        {
            PROFILE_SCOPE("PixelBufferBorderDraw");
            Renderer::GetInst().DrawScreenBorderPxBuff(0xF);
        }
        {
            PROFILE_SCOPE("PixelBufferOutput");
            Screen::GetInst().OutputBuffer();
        }

        FPSClock::GetInst().EndFPSClock();
        
        // profiling work
        Profiler::GetInst().EndFrame();
        frameCounter++;
        if (frameCounter % 60 == 0) { 
            frameCounter = 0;
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
                player->Update();
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
    switch (gameState) {
        case GameState::Playing:
            RenderPlaying();
            break;
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