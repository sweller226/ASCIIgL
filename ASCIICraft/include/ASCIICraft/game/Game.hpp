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

// block update systems
#include <ASCIICraft/ecs/systems/blockupdate/BlockUpdateSystem.hpp>
#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>
#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>

// event systems
#include <ASCIICraft/events/EventBus.hpp>

// events
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>

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
    
private:
    // Resources
    entt::registry registry;
    EventBus eventBus;
    std::unique_ptr<ASCIIgL::Texture> blockAtlas;  // Block texture atlas - must persist during game lifetime

    // ecs systems
    ecs::systems::MovementSystem movementSystem;
    ecs::systems::CameraSystem cameraSystem;
    ecs::systems::PhysicsSystem physicsSystem;
    ecs::systems::RenderSystem renderSystem;

    // block updates
    ecs::systems::BlockUpdateSystem blockUpdateSystem;
    ecs::systems::MiningSystem miningSystem;
    ecs::systems::PlacingSystem placingSystem;

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
