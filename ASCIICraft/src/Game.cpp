#include <ASCIICraft/game/Game.hpp>

#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Palette.hpp>

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

    // std::array<PaletteEntry, Palette::COLOR_COUNT> paletteEntries = {{
    //     { {0.0f, 0.0f, 0.0f}, 0, 0x0 }, // Black
    //     { {0.7765f, 0.7765f, 0.7765f}, 1, 0x1 }, // Stone Light Gray
    //     { {0.5882f, 0.5882f, 0.5882f}, 2, 0x2 }, // Smooth Stone Gray
    //     { {0.4902f, 0.4902f, 0.4902f}, 3, 0x3 }, // Cobblestone Gray
    //     { {0.3451f, 0.3451f, 0.3451f}, 4, 0x4 }, // Shady Rock Gray
    //     { {0.2353f, 0.2353f, 0.2353f}, 5, 0x5 }, // Deep Rock Gray

    //     { {0.4196f, 0.6510f, 1.0000f}, 6, 0x6 }, // Sky Blue

    //     { {0.3725f, 0.6235f, 0.2078f}, 7, 0x7 }, // Grass Green
    //     { {0.2510f, 0.4275f, 0.1255f}, 8, 0x8 }, // Leaf Green
    //     { {0.1569f, 0.2745f, 0.0784f}, 9, 0x9 }, // Dark Foliage Green
    //     { {0.4941f, 0.7843f, 0.3137f}, 10, 0xA }, // Bright Grass Tip Green

    //     { {0.4000f, 0.3020f, 0.1804f}, 11, 0xB }, // Oak Log Brown
    //     { {0.6353f, 0.5098f, 0.3098f}, 12, 0xC }, // Plank Brown
    //     { {0.5216f, 0.3765f, 0.2588f}, 13, 0xD }, // Dirt Brown
    //     { {0.3137f, 0.2039f, 0.1294f}, 14, 0xE }, // Dark Wood Brown
    //     { {0.2000f, 0.1200f, 0.0600f}, 15, 0xF }  // Deep Brown
    // }};

    std::array<PaletteEntry, Palette::COLOR_COUNT> paletteEntries = {{
        { {0.0000f, 0.0000f, 0.0000f}, 0, 0x0 },  // Black
        { {0.0667f, 0.0667f, 0.0667f}, 1, 0x1 },  // Very Dark Gray
        { {0.1333f, 0.1333f, 0.1333f}, 2, 0x2 },  // Darker Gray
        { {0.2000f, 0.2000f, 0.2000f}, 3, 0x3 },  // Dark Gray
        { {0.2667f, 0.2667f, 0.2667f}, 4, 0x4 },  // Charcoal Gray
        { {0.3333f, 0.3333f, 0.3333f}, 5, 0x5 },  // Medium Dark Gray
        { {0.4000f, 0.4000f, 0.4000f}, 6, 0x6 },  // Mid Gray
        { {0.4667f, 0.4667f, 0.4667f}, 7, 0x7 },  // Soft Gray
        { {0.5333f, 0.5333f, 0.5333f}, 8, 0x8 },  // Neutral Gray
        { {0.6000f, 0.6000f, 0.6000f}, 9, 0x9 },  // Light Neutral Gray
        { {0.6667f, 0.6667f, 0.6667f}, 10, 0xA }, // Light Gray
        { {0.7333f, 0.7333f, 0.7333f}, 11, 0xB }, // Brighter Gray
        { {0.8000f, 0.8000f, 0.8000f}, 12, 0xC }, // Pale Gray
        { {0.8667f, 0.8667f, 0.8667f}, 13, 0xD }, // Very Pale Gray
        { {0.9333f, 0.9333f, 0.9333f}, 14, 0xE }, // Near White
        { {1.0000f, 1.0000f, 1.0000f}, 15, 0xF }  // White
    }};

    // std::array<PaletteEntry, Palette::COLOR_COUNT> paletteEntries = {{
    //     { {0.00f, 0.00f, 0.00f}, 0, 0x0 },    // Black - 0
    //     { {0.00f, 0.00f, 1.00f}, 1, 0x1 },    // Blue - 4
    //     { {0.00f, 1.00f, 0.00f}, 2, 0x2 },    // Green - 2
    //     { {1.00f, 0.00f, 0.00f}, 3, 0x3 },    // Red - 6
    //     { {1.00f, 1.00f, 0.00f}, 4, 0x4 },    // Yellow - 1
    //     { {0.00f, 1.00f, 1.00f}, 5, 0x5 },    // Cyan - 5
    //     { {1.00f, 0.00f, 1.00f}, 6, 0x6 },    // Magenta - 3
    //     { {1.00f, 1.00f, 1.00f}, 7, 0x7 },    // White - 7
    //     { {0.00f, 0.00f, 0.00f}, 8, 0x8 },    // Black
    //     { {0.00f, 0.00f, 0.00f}, 9, 0x9 },    // Black
    //     { {0.00f, 0.00f, 0.00f}, 10, 0xA },    // Black
    //     { {0.00f, 0.00f, 0.00f}, 11, 0xB },    // Black
    //     { {0.00f, 0.00f, 0.00f}, 12, 0xC },    // Black
    //     { {0.00f, 0.00f, 0.00f}, 13, 0xD },    // Black
    //     { {0.00f, 0.00f, 0.00f}, 14, 0xE },    // Black
    //     { {0.00f, 0.00f, 0.00f}, 15, 0xF },    // Black
    // }};
    Palette gamePalette = Palette(paletteEntries); // Default palette

    const int screenInitResult = Screen::GetInstance().InitializeScreen(SCREEN_WIDTH, SCREEN_HEIGHT, L"ASCIICraft", FONT_SIZE, static_cast<unsigned int>(TARGET_FPS), 1.0f, 0x6, gamePalette);
    Renderer::SetWireframe(false);
    Renderer::SetBackfaceCulling(true);
    Renderer::SetCCW(true);
	Renderer::SetAntialiasingsamples(4);
	Renderer::SetAntialiasing(true);

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

    GuiManager guiManager = GuiManager(SCREEN_WIDTH, SCREEN_HEIGHT);
    // Texture grassTexture = Texture("res/textures/green-grass-close-up.png");
    Texture grassTexture = Texture("res/textures/blue-gradient.jpg");

    // Main game loop
    while (isRunning) {
        Screen::GetInstance().StartFPSClock();
        Screen::GetInstance().ClearBuffer();
        
        HandleInput();
        Update();
        // Renderer::Draw2DQuadPercSpace(guiManager.GetVShader(), grassTexture, glm::vec2(0.625, 0.625), 0.0f, glm::vec2(0.25f, 0.25f), guiManager.GetCamera(), 0);
        Render();
        // Renderer::TestRenderColor();
        Screen::GetInstance().OutputBuffer();
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