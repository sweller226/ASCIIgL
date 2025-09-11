#include <ASCIICraft/game/Game.hpp>
#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIICraft/world/Block.hpp>

Game::Game() 
    : gameState(GameState::MainMenu)
    , isRunning(false)
    , targetFPS(TARGET_FPS) {
}

Game::~Game() {
    Shutdown();
}

bool Game::Initialize() {
    Logger::Info("Initializing ASCIICraft...");
    
    // Initialize screen
    if (!InitializeScreen()) {
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
    player = std::make_unique<Player>(glm::vec3(0.0f, 70.0f, 0.0f)); // Spawn at height 70
    inputManager = std::make_unique<InputManager>();
    
    // Connect player to world
    world->SetPlayer(player.get());
    world->SetRenderDistance(8); // 8 chunk render distance
    
    // Generate some initial chunks around spawn
    for (int x = -2; x <= 2; x++) {
        for (int y = 0; y <= 8; y++) { // Y: 0-127 (8 chunks high)
            for (int z = -2; z <= 2; z++) {
                world->GenerateEmptyChunk(ChunkCoord(x, y, z));
            }
        }
    }
    
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
        Screen::StartFPSClock();
        
        float deltaTime = Screen::GetDeltaTime();
        
        HandleInput();
        Update(deltaTime);
        Render();
        
        Screen::EndFPSClock();
    }
    
    Shutdown();
}

void Game::Update(float deltaTime) {
    switch (gameState) {
        case GameState::Playing:
            UpdateGame(deltaTime);
            break;
        case GameState::Exiting:
            isRunning = false;
            break;
    }
}

void Game::Render() {
    Screen::ClearBuffer();
    
    switch (gameState) {
        case GameState::Playing:
            RenderGame();
            break;
    }
    
    Screen::OutputBuffer();
}

void Game::HandleInput() {
    inputManager->Update();
    
    // Global input (works in all states)
    if (inputManager->IsKeyPressed(Key::ESCAPE)) {
        if (gameState == GameState::Playing) {
            gameState = GameState::Paused;
        } else if (gameState == GameState::Paused) {
            gameState = GameState::Playing;
        } else if (gameState == GameState::MainMenu) {
            isRunning = false;
        }
    }
    
    // State-specific input
    if (gameState == GameState::Playing) {
        player->HandleInput(*inputManager, Screen::GetDeltaTime());
    }
}

void Game::Shutdown() {
    Logger::Info("Shutting down ASCIICraft...");
    
    // Clean up resources
    world.reset();
    player.reset();
    inputManager.reset();
    blockAtlas.reset();
    
    Screen::Cleanup();
    Logger::Info("ASCIICraft shutdown complete");
}

bool Game::InitializeScreen() {
    int result = Screen::InitializeScreen(
        SCREEN_WIDTH,
        SCREEN_HEIGHT, 
        L"ASCIICraft - Minecraft in ASCII",
        FONT_SIZE,
        static_cast<unsigned int>(TARGET_FPS),
        1.0f,
        BG_BLACK
    );
    
    return result == SCREEN_NOERROR;
}

bool Game::LoadResources() {
    Logger::Info("Loading game resources...");
    
    // Load block texture atlas
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

void Game::UpdateGame(float deltaTime) {
    // Update world (chunk loading, physics, etc.)
    world->Update(deltaTime);
    
    // Update player (movement, physics, input)
    player->Update(deltaTime);
    player->UpdatePhysics(deltaTime, *world);
    player->UpdateCamera();
}

void Game::RenderGame() {
    // Get visible chunks for rendering
    glm::vec3 playerPos = player->GetPosition();
    glm::vec3 viewDir = glm::vec3(0.0f, 0.0f, -1.0f); // TODO: Get from camera
    
    auto visibleChunks = world->GetVisibleChunks(playerPos, viewDir);
}