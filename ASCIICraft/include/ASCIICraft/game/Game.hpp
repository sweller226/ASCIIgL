#pragma once

#include <memory>

#include <entt/entt.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/world/World.hpp>

// ecs managers
#include <ASCIICraft/ecs/managers/PlayerManager.hpp>

// ecs systems
#include <ASCIICraft/ecs/systems/MovementSystem.hpp> 
#include <ASCIICraft/ecs/systems/CameraSystem.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp> 
#include <ASCIICraft/ecs/systems/PhysicsSystem.hpp> 

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
    void Update();
    void Render();
    void HandleInput();
    
private:
    // Resources
    entt::registry registry;
    std::unique_ptr<ASCIIgL::Texture> blockAtlas;  // Block texture atlas - must persist during game lifetime

    // ecs systems
    ecs::systems::MovementSystem movementSystem;
    ecs::systems::PhysicsSystem physicsSystem;
    ecs::systems::CameraSystem cameraSystem;
    ecs::systems::RenderSystem renderSystem;
    
    // Game state
    GameState gameState;
    bool isRunning;
    
    // Private methods
    bool LoadResources();
    void InitializeContext();
    void InitializeSystems();
    void RenderPlaying();
    
    // Constants
    static inline int SCREEN_WIDTH = 550;
    static inline int SCREEN_HEIGHT = 350;
    static constexpr int FONT_SIZE = 4;
    static constexpr float TARGET_FPS = 60.0f;
};
