#pragma once

#include <memory>
#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/player/Player.hpp>
#include <ASCIICraft/input_manager/InputManager.hpp>

enum class GameState {
    Playing,
    Exiting
};

class Game {
public:
    Game();
    ~Game();
    
    // Core game functions
    bool Initialize();
    void Run();
    void Shutdown();
    
    // Game state management
    void SetGameState(GameState state) { gameState = state; }
    GameState GetGameState() const { return gameState; }
    
    // Game loop components
    void Update(float deltaTime);
    void Render();
    void HandleInput();
    
private:
    // Core systems
    std::unique_ptr<World> world;
    std::unique_ptr<Player> player;
    std::unique_ptr<InputManager> inputManager;
    
    // Rendering
    std::unique_ptr<Texture> blockAtlas;
    
    // Game state
    GameState gameState;
    bool isRunning;
    float targetFPS;
    
    // Private methods
    bool InitializeScreen();
    bool LoadResources();
    void UpdateGame(float deltaTime);
    void RenderGame();
    void ProcessInput();
    
    // Constants
    static constexpr int SCREEN_WIDTH = 120;
    static constexpr int SCREEN_HEIGHT = 40;
    static constexpr int FONT_SIZE = 16;
    static constexpr float TARGET_FPS = 60.0f;
};
