#pragma once

#include <memory>
#include <functional>

#include <entt/entt.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/renderer/Shader.hpp>

#include <ASCIICraft/world/World.hpp>

// ecs factories
#include <ASCIICraft/ecs/factories/PlayerFactory.hpp>

// ecs systems
#include <ASCIICraft/ecs/systems/MovementSystem.hpp>
#include <ASCIICraft/ecs/systems/CameraSystem.hpp>
#include <ASCIICraft/input/InputSystem.hpp>
#include <ASCIICraft/input/GameplayInputFilter.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>
#include <ASCIICraft/ecs/systems/PhysicsSystem.hpp>
#include <ASCIICraft/gui/GuiManager.hpp>

// block update systems
#include <ASCIICraft/ecs/systems/blockupdate/BlockUpdateSystem.hpp>
#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>
#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>

// event systems
#include <ASCIIgL/util/EventBus.hpp>

// events
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>
#include <ASCIICraft/events/InputEvents.hpp>

enum class GameState {
    Playing,
    Exiting
};

class Game {
public:
    Game();
    ~Game();
    
    // Core game functions
    bool Initialize(bool renderToTerminal = true);
    void Run(std::function<bool()> shouldExternalExit, bool renderToTerminal = true);
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
    ASCIIgL::EventBus eventBus;
    
    // ecs systems (inputSystem first; gameplayInputFilter wraps it for movement/camera so GUI blocks input there)
    input::InputSystem inputSystem;
    GameplayInputFilter gameplayInputFilter;
    ecs::systems::MovementSystem movementSystem;
    ecs::systems::CameraSystem cameraSystem;
    ecs::systems::PhysicsSystem physicsSystem;
    ecs::systems::RenderSystem renderSystem;

    gui::GuiManager guiManager;

    // block updates
    ecs::systems::BlockUpdateSystem blockUpdateSystem;
    ecs::systems::MiningSystem miningSystem;
    ecs::systems::PlacingSystem placingSystem;

    // ecs factories
    ecs::factories::PlayerFactory playerFactory;

    // Game state
    GameState gameState;
    std::function<bool()> shouldExternalExit;
    bool shouldInternalExit;
    /// Prevents duplicate teardown if \ref Shutdown is invoked from multiple paths (e.g. destructor + explicit call).
    bool shutdownInvoked_ = false;
    
    // Private methods
    bool LoadResources();
    bool LoadTextures();
    void InitializeWorld();
    void InitializePlayer();
    void InitializeSystems();
    void RenderPlaying();
    void InitializeItemDefinitions();
    void InitializeBlockStates();
    
    // Constants
    static inline int SCREEN_WIDTH = 500;
    static inline int SCREEN_HEIGHT = 250;
    static constexpr float FONT_SIZE = 3.0f;
    static constexpr float TARGET_FPS = 60.0f;
};
