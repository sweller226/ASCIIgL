#pragma once

#include <memory>
#include <vector>

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/gui/GUISurfaceLibrary.hpp>
#include <ASCIICraft/gui/GUISurface.hpp>
#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/input/IInputSource.hpp>

namespace ASCIIgL { class EventBus; }

namespace gui {

/// Central OOP GUI: screen stack, cursor, and 2D camera.
/// Game calls Update(), then Render() to draw GUI via GUIRenderer (separate from EntityRenderSystem).
class GUIManager {
public:
    GUIManager(entt::registry& registry, ASCIIgL::EventBus& eventBus, IInputSource& input);

    glm::vec2 GetScreenSize() const { return m_screenSize; }

    void Update();
    void Render();

    void SetActive2DCamera(ASCIIgL::Camera2D* camera2D);
    /// Builds cursor mesh/material from "cursorTexture". Call after guiMaterial is registered.
    void BuildCursorSurface();
    void SetBaseScreen(GUIScreen* screen);

    /// Push \p screen if it is not the top screen; otherwise pop it.
    /// Requires that a base screen is already set (stack size >= 1).
    void ToggleScreen(GUIScreen* screen);

    /// True when the inventory screen is on the stack (e.g. after pressing E).
    bool IsBlockingInput() const;
    bool IsTopScreen(const GUIScreen* screen) const;
    /// Screen-pixel hotspot (center of cursor sprite). Used for hit-test, clicks, and movement.
    glm::vec2 GetCursorHotspot() const { return m_cursorHotspot; }
    GUISurfaceLibrary& GetMeshLibrary() { return m_meshLibrary; }
    const GUISurfaceLibrary& GetMeshLibrary() const { return m_meshLibrary; }

    void PushScreen(GUIScreen* screen);
    void PopScreen();

private:
    void UpdateCursor(GUIScreen* top);
    /// Top-left of the cursor/carried-item cell for RenderGUIQuad (hotspot − size/2).
    glm::vec2 GetCursorDrawTopLeft() const;

    entt::registry& m_registry;
    ASCIIgL::EventBus& m_eventBus;
    IInputSource& m_input;

    glm::vec2 m_screenSize{0.0f, 0.0f};
    glm::vec2 m_cursorHotspot{0.0f, 0.0f};
    glm::vec2 m_cursorSize{16.0f, 16.0f};
    float m_cursorMoveSpeed = 56.0f;
    GUISurface m_cursorSurface{};
    
    std::vector<GUIScreen*> m_screenStack;

    ASCIIgL::Camera2D* m_guiCamera = nullptr;
    std::unique_ptr<GUIRenderer> m_renderer;
    GUISurfaceLibrary m_meshLibrary;
};

} // namespace gui
