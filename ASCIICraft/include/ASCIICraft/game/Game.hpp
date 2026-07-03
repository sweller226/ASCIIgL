#pragma once

#include <memory>
#include <functional>

#include <entt/entt.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/renderer/Shader.hpp>

#include <ASCIICraft/world/World.hpp>

// ecs factories
#include <ASCIICraft/ecs/factories/PlayerFactory.hpp>

// ecs systems
#include <ASCIICraft/ecs/systems/MovementSystem.hpp>
#include <ASCIICraft/ecs/systems/CameraSystem.hpp>
#include <ASCIICraft/input/InputSystem.hpp>
#include <ASCIICraft/input/GameplayInputFilter.hpp>
#include <ASCIICraft/ecs/systems/EntityRenderSystem.hpp>
#include <ASCIICraft/ecs/systems/HeldItemRenderSystem.hpp>
#include <ASCIICraft/ecs/systems/PhysicsSystem.hpp>
#include <ASCIICraft/ecs/systems/LifetimeSystem.hpp>
#include <ASCIICraft/ecs/systems/ParticleSystem.hpp>
#include <ASCIICraft/ecs/systems/sound/MusicSystem.hpp>
#include <ASCIICraft/ecs/systems/sound/SoundSystem.hpp>
#include <ASCIICraft/ecs/systems/sound/StepSFXSystem.hpp>
#include <ASCIICraft/ecs/systems/ViewBobbingSystem.hpp>

// GUI
#include <ASCIICraft/gui/GUIManager.hpp>

// block update systems
#include <ASCIICraft/ecs/systems/blockupdate/BlockUpdateSystem.hpp>
#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>
#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>
#include <ASCIICraft/ecs/systems/InventorySystem.hpp>
#include <ASCIICraft/ecs/systems/DroppedItemSystem.hpp>
#include <ASCIICraft/ecs/systems/HotbarSystem.hpp>
#include <ASCIICraft/ecs/systems/BlockTargetSystem.hpp>

// event systems
#include <ASCIIgL/util/EventBus.hpp>

// events
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIICraft/events/GUIEvents.hpp>

enum class GameState {
    Playing,
    Exiting
};

namespace gui { class PlayHUDScreen; }
namespace gui { class InventoryScreen; }

class Game {
public:
    Game();
    ~Game();
    
    // Core game functions
    bool Initialize(bool renderToTerminal = true, bool multicolor = false);
    void Run(std::function<bool()> shouldExternalExit, bool renderToTerminal = true, bool multicolor = false);
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
    ecs::systems::BlockTargetSystem blockTargetSystem;
    ecs::systems::EntityRenderSystem entityRenderSystem;
    ecs::systems::HeldItemRenderSystem heldItemRenderSystem;
    ecs::systems::LifetimeSystem lifetimeSystem;
    ecs::systems::ParticleSystem particleSystem;
    ecs::systems::SoundSystem soundSystem;
    ecs::systems::MusicSystem musicSystem;
    ecs::systems::StepSFXSystem stepSfxSystem;
    ecs::systems::ViewBobbingSystem viewBobbingSystem;

    // GUI screens
    gui::GUIManager guiManager;
    std::unique_ptr<gui::PlayHUDScreen> playHudScreen_;
    std::unique_ptr<gui::InventoryScreen> inventoryScreen_;

    // block updates
    ecs::systems::BlockUpdateSystem blockUpdateSystem;
    ecs::systems::MiningSystem miningSystem;
    ecs::systems::PlacingSystem placingSystem;
    ecs::systems::InventorySystem inventorySystem;
    ecs::systems::DroppedItemSystem droppedItemSystem;
    ecs::systems::HotbarSystem hotbarSystem;

    // ecs factories
    ecs::factories::PlayerFactory playerFactory;

    std::unique_ptr<ASCIIgL::Camera2D> guiCamera;
    std::unique_ptr<ASCIIgL::Camera3D> heldItemViewCamera;

    // Game state
    GameState gameState;
    std::function<bool()> shouldExternalExit;
    bool shouldInternalExit;
    /// Prevents duplicate teardown if \ref Shutdown is invoked from multiple paths (e.g. destructor + explicit call).
    bool shutdownInvoked_ = false;
    
    // Loading
    bool LoadResources();
    bool LoadTerrainMaterial();
    bool LoadDroppedItemMaterial();
    bool LoadHeldItemMaterial();
    bool LoadGUIMaterial();
    bool LoadGUIItemMaterial();
    bool LoadGUIBlockMaterial();
    bool LoadGUITextMaterial();
    bool LoadBlockTargetOutlineMaterial();
    bool LoadFont();

    bool LoadTextures(bool multicolor);
    void InitializeWorld();
    void InitializePlayer();
    void InitializeSystems();
    void InitializeGUI();
    void RenderPlaying();
    void InitializeItemDefinitions();
    void InitializeBlockStates();
    
    // Constants
    static inline int SCREEN_WIDTH = 500;
    static inline int SCREEN_HEIGHT = 250;
    static constexpr bool SUPERSAMPLE_2X = true;
    static constexpr float FONT_SIZE = 3.0f;
    static constexpr float TARGET_FPS = 60.0f;
};
