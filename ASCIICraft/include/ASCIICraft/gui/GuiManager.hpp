#pragma once

#include <memory>
#include <vector>

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/gui/Screen.hpp>
#include <ASCIICraft/input/IInputSource.hpp>

class EventBus;

namespace ecs::systems { class RenderSystem; }

namespace ASCIICraft::gui {

/// Central OOP GUI: screen stack, cursor, and 2D camera.
/// Game calls Update(), then Draw(renderSystem) to add GUI items directly to RenderSystem.
class GuiManager {
public:
    GuiManager(entt::registry& registry, EventBus& eventBus, ASCIICraft::IInputSource& input);

    void SetScreenSize(glm::vec2 size);
    glm::vec2 GetScreenSize() const { return m_screenSize; }

    void SetPlayScreen(std::unique_ptr<Screen> screen);
    void SetInventoryScreen(std::unique_ptr<Screen> screen);
    void ToggleInventoryScreen();

    void Update();
    /// Adds GUI items directly to RenderSystem. Call after RenderSystem::BeginFrame(), before RenderSystem::Render().
    void Draw(::ecs::systems::RenderSystem& renderSystem);

    /// For RenderSystem. May be null until SetScreenSize has been called.
    ASCIIgL::Camera2D* GetCamera2D() { return m_guiCamera.get(); }
    const ASCIIgL::Camera2D* GetCamera2D() const { return m_guiCamera.get(); }

    bool IsBlockingInput() const;
    /// True when the inventory screen is on the stack (e.g. after pressing E).
    bool IsInventoryOpen() const;
    glm::vec2 GetCursorPosition() const { return m_cursorPosition; }

private:
    void PushScreen(Screen* screen);
    void PopScreen();

    entt::registry& m_registry;
    EventBus& m_eventBus;
    ASCIICraft::IInputSource& m_input;

    glm::vec2 m_screenSize{550.0f, 350.0f};
    glm::vec2 m_cursorPosition{0.0f, 0.0f};
    glm::vec2 m_cursorSize{4.0f, 4.0f};
    float m_cursorMoveSpeed = 8.0f;

    std::unique_ptr<Screen> m_playScreen;
    std::unique_ptr<Screen> m_inventoryScreen;
    std::vector<Screen*> m_screenStack;

    std::unique_ptr<ASCIIgL::Camera2D> m_guiCamera;
};

} // namespace ASCIICraft::gui
