#pragma once

#include <memory>

#include <entt/entt.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/renderer/gpu/Shader.hpp>

#include <ASCIICraft/world/World.hpp>

// ecs factories
#include <ASCIICraft/ecs/factories/PlayerFactory.hpp>

// ecs systems
#include <ASCIICraft/ecs/systems/MovementSystem.hpp> 
#include <ASCIICraft/ecs/systems/CameraSystem.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp> 
#include <ASCIICraft/ecs/systems/PhysicsSystem.hpp>
#include <ASCIICraft/ecs/systems/GUISystem.hpp> 

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
    
    // ecs systems
    ecs::systems::MovementSystem movementSystem;
    ecs::systems::CameraSystem cameraSystem;
    ecs::systems::PhysicsSystem physicsSystem;
    ecs::systems::RenderSystem renderSystem;
    ecs::systems::GUISystem guiSystem;

    // block updates
    ecs::systems::BlockUpdateSystem blockUpdateSystem;
    ecs::systems::MiningSystem miningSystem;
    ecs::systems::PlacingSystem placingSystem;

    // ecs factories
    ecs::factories::PlayerFactory playerFactory;

    // Game state
    GameState gameState;
    bool isRunning;
    
    // Private methods
    bool LoadResources();
    void InitializeContext();
    void InitializeSystems();
    void RenderPlaying();
    void InitializeItemDefinitions();
    
    // Constants
    static inline int SCREEN_WIDTH = 550;
    static inline int SCREEN_HEIGHT = 350;
    static constexpr int FONT_SIZE = 4;
    static constexpr float TARGET_FPS = 60.0f;
};
