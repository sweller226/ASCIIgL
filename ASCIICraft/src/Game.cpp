#include <ASCIICraft/game/Game.hpp>

#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

#include <ASCIICraft/world/Block.hpp>

Game::Game() 
    : gameState(GameState::Playing)
    , isRunning(false) {
}

Game::~Game() {
    Shutdown();
}

bool Game::Initialize() {
    Logger::Info("Initializing ASCIICraft...");

    const int screenInitResult = Screen::GetInstance().InitializeScreen(SCREEN_WIDTH, SCREEN_HEIGHT, L"ASCIICraft", FONT_SIZE, static_cast<unsigned int>(TARGET_FPS), 1.0f, BG_BLUE);
    Renderer::SetWireframe(false);
    Renderer::SetBackfaceCulling(true);
    Renderer::SetCCW(true);
	Renderer::SetAntialiasingsamples(4);
	Renderer::SetAntialiasing(true);
	Renderer::SetGrayscale(false);

    // Initialize screen
    if (screenInitResult != SCREEN_NOERROR) {
        Logger::Error("Failed to initialize screen");
        return false;
    }
    
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
    while (isRunning) {
        Screen::GetInstance().StartFPSClock();
        Screen::GetInstance().ClearBuffer();
        
        HandleInput();
        Update();
        Render();
        
        Screen::GetInstance().EndFPSClock();
        Screen::GetInstance().RenderTitle(true);
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
    
    Screen::GetInstance().OutputBuffer();
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
    
    Screen::GetInstance().Cleanup();
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