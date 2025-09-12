#include <ASCIICraft/game/Game.hpp>
#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIIgL/renderer/Screen.hpp>
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

    const int screenInitResult = Screen::GetInstance().InitializeScreen(SCREEN_WIDTH, SCREEN_HEIGHT, L"I Don't Wanna Run For Christmas", FONT_SIZE, static_cast<unsigned int>(TARGET_FPS), 1.0f, BG_BLUE);

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
    world = std::make_unique<World>();
    inputManager = std::make_unique<InputManager>();

    world->SetRenderDistance(2); // 2 chunk render distance
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
        
        float deltaTime = Screen::GetInstance().GetDeltaTime();
        
        HandleInput();
        Update(deltaTime);
        Render();
        
        Screen::GetInstance().EndFPSClock();
    }
    
    Shutdown();
}

void Game::Update(float deltaTime) {
    switch (gameState) {
        case GameState::Playing:
            // Update world (chunk loading, physics, etc.)
            world->Update(deltaTime);
            break;
        case GameState::Exiting:
            isRunning = false;
            break;
    }
}

void Game::Render() {
    Screen::GetInstance().ClearBuffer();
    
    switch (gameState) {
        case GameState::Playing:
            RenderPlaying();
            break;
    }
    
    Screen::GetInstance().OutputBuffer();
}

void Game::HandleInput() {
    inputManager->Update();
    
    // Handle exit input
    if (inputManager->IsActionPressed("quit")) {
        gameState = GameState::Exiting;
    }
}

void Game::Shutdown() {
    Logger::Info("Shutting down ASCIICraft...");
    
    // Clean up resources
    world.reset();
    inputManager.reset();
    
    Screen::GetInstance().Cleanup();
    Logger::Info("ASCIICraft shutdown complete");
}

bool Game::LoadResources() {
    Logger::Info("Loading game resources...");
    
    // Load block texture atlas
    std::unique_ptr<Texture> blockAtlas = std::make_unique<Texture>("res/textures/terrain.png");
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