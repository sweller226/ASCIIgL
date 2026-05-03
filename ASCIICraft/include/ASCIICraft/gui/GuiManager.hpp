#pragma once

#include <memory>
#include <vector>

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/gui/GUISurfaceLibrary.hpp>
#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/input/IInputSource.hpp>

namespace ASCIIgL { class EventBus; }

namespace gui {

/// Central OOP GUI: screen stack, cursor, and 2D camera.
/// Game calls Update(), then Draw(ecsRenderSystem) to add GUI items directly to RenderSystem.
class GUIManager {
public:
    GUIManager(entt::registry& registry, ASCIIgL::EventBus& eventBus, IInputSource& input);

    glm::vec2 GetScreenSize() const { return m_screenSize; }

    void Update();
    void Render();

    void SetActive2DCamera(ASCIIgL::Camera2D* camera2D) { m_guiCamera = camera2D; };

    /// True when the inventory screen is on the stack (e.g. after pressing E).
    bool IsBlockingInput() const;
    glm::vec2 GetCursorPosition() const { return m_cursorPosition; }
    GUISurfaceLibrary& GetMeshLibrary() { return m_meshLibrary; }
    const GUISurfaceLibrary& GetMeshLibrary() const { return m_meshLibrary; }

private:
    void PushScreen(GUIScreen* screen);
    void PopScreen();

    entt::registry& m_registry;
    ASCIIgL::EventBus& m_eventBus;
    IInputSource& m_input;

    glm::vec2 m_screenSize{550.0f, 350.0f};
    glm::vec2 m_cursorPosition{0.0f, 0.0f};
    glm::vec2 m_cursorSize{4.0f, 4.0f};
    float m_cursorMoveSpeed = 8.0f;
    
    std::vector<GUIScreen*> m_screenStack;

    ASCIIgL::Camera2D* m_guiCamera;
    GUISurfaceLibrary m_meshLibrary;
};

} // namespace gui
