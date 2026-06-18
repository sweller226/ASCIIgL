#include <ASCIICraft/gui/GUIManager.hpp>
#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/gui/GuiSlotHover.hpp>
#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/gui/GUISurface.hpp>
#include <ASCIICraft/events/ItemEvents.hpp>
#include <ASCIICraft/gui/GuiItemIcon.hpp>
#include <ASCIICraft/ecs/components/ItemCarried.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <algorithm>
#include <glm/vec2.hpp>

namespace gui {

namespace {

void SetGuiSlotHover(entt::registry& registry, const GuiSlotHover& hover) {
    if (auto* ctx = registry.ctx().find<GuiSlotHover>()) {
        *ctx = hover;
    } else {
        registry.ctx().emplace<GuiSlotHover>(hover);
    }
}

} // namespace

GUIManager::GUIManager(entt::registry& registry, ASCIIgL::EventBus& eventBus, IInputSource& input)
    : m_registry(registry)
    , m_eventBus(eventBus)
    , m_input(input)
{}

glm::vec2 GUIManager::GetCursorDrawTopLeft() const {
    return m_cursorHotspot - m_cursorSize * 0.5f;
}

void GUIManager::PushScreen(GUIScreen* screen) {
    if (screen)
        m_screenStack.push_back(screen);
}

void GUIManager::PopScreen() {
    if (m_screenStack.size() > 1)
        m_screenStack.pop_back();
}

void GUIManager::Update() {
    const unsigned screenW = ASCIIgL::Screen::GetInst().GetWidth();
    const unsigned screenH = ASCIIgL::Screen::GetInst().GetHeight();
    m_screenSize = {static_cast<float>(screenW), static_cast<float>(screenH)};
    if (m_guiCamera) {
        m_guiCamera->setScreenDimensions(screenW, screenH);
    }

    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    if (m_screenStack.empty()) return;

    for (GUIScreen* screen : m_screenStack) {
        screen->Layout(m_screenSize, nullptr);
        screen->Update(dt);
    }

    GUIScreen* top = m_screenStack.back();
    if (top->blocksInput) {
        UpdateCursor(top);
    } else {
        SetGuiSlotHover(m_registry, {});
    }
}

void GUIManager::Render() {
    if (!m_renderer) return;

    for (GUIScreen* screen : m_screenStack) {
        screen->Draw(*m_renderer);
    }

    if (!m_screenStack.empty() && m_screenStack.back()->blocksInput) {
        const glm::vec2 cursorTopLeft = GetCursorDrawTopLeft();
        const entt::entity player = ecs::components::GetPlayerEntity(m_registry);
        if (player != entt::null && m_registry.valid(player)) {
            if (m_cursorSurface.mesh && m_cursorSurface.material) {
                m_renderer->RenderGUIQuad(
                    cursorTopLeft, m_cursorSize, 9998, m_cursorSurface.mesh, m_cursorSurface.material);
            }

            if (const auto* carried = m_registry.try_get<ecs::components::ItemCarried>(player)) {
                DrawItemStackIcon(m_registry, *m_renderer, carried->stack, cursorTopLeft, m_cursorSize, 9999);
            }
        } else if (m_cursorSurface.mesh && m_cursorSurface.material) {
            m_renderer->RenderGUIQuad(
                cursorTopLeft, m_cursorSize, 10000, m_cursorSurface.mesh, m_cursorSurface.material);
        }
    }
}

void GUIManager::SetActive2DCamera(ASCIIgL::Camera2D* camera2D) {
    m_guiCamera = camera2D;
    if (m_guiCamera) {
        m_renderer = std::make_unique<GUIRenderer>(*m_guiCamera);
    } else {
        m_renderer.reset();
    }
}

void GUIManager::BuildCursorSurface() {
    auto cursorTexture = ASCIIgL::TextureLibrary::GetInst().GetTexture("cursorTexture");
    if (!cursorTexture) {
        m_cursorSurface = {};
        return;
    }
    m_cursorSurface = GUISurface::FromTexture(cursorTexture, "guiMaterial.cursor");
}

void GUIManager::SetBaseScreen(GUIScreen* screen) {
    m_screenStack.clear();
    PushScreen(screen);
}

bool GUIManager::IsBlockingInput() const {
    if (m_screenStack.empty()) return false;
    return m_screenStack.back()->blocksInput;
}

bool GUIManager::IsTopScreen(const GUIScreen* screen) const {
    if (m_screenStack.empty()) return false;
    return m_screenStack.back() == screen;
}

void GUIManager::ToggleScreen(GUIScreen* screen) {
    if (!screen) return;
    if (IsTopScreen(screen)) {
        PopScreen();
    } else {
        PushScreen(screen);
        const unsigned screenW = ASCIIgL::Screen::GetInst().GetWidth();
        const unsigned screenH = ASCIIgL::Screen::GetInst().GetHeight();
        const glm::vec2 screenSize{static_cast<float>(screenW), static_cast<float>(screenH)};
        glm::vec2 initialHotspot{};
        if (screen->TryGetInitialCursorPosition(screenSize, initialHotspot)) {
            m_cursorHotspot = initialHotspot;
        }
    }
}

void GUIManager::UpdateCursor(GUIScreen* top) {
    if (!top || !top->blocksInput) return;

    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    const float cursorStep = m_cursorMoveSpeed * dt;

    if (m_input.IsActionHeld("camera_left"))  m_cursorHotspot.x -= cursorStep;
    if (m_input.IsActionHeld("camera_right")) m_cursorHotspot.x += cursorStep;
    if (m_input.IsActionHeld("camera_up"))    m_cursorHotspot.y -= cursorStep;
    if (m_input.IsActionHeld("camera_down"))  m_cursorHotspot.y += cursorStep;

    const glm::vec2 halfSize = m_cursorSize * 0.5f;
    const glm::vec2 minHotspot = halfSize;
    const glm::vec2 maxHotspot{
        std::max(halfSize.x, m_screenSize.x - halfSize.x),
        std::max(halfSize.y, m_screenSize.y - halfSize.y)
    };
    m_cursorHotspot.x = std::clamp(m_cursorHotspot.x, minHotspot.x, maxHotspot.x);
    m_cursorHotspot.y = std::clamp(m_cursorHotspot.y, minHotspot.y, maxHotspot.y);

    if (m_input.IsActionPressed("quit"))
        PopScreen();

    top->OnCursorMove(m_cursorHotspot);

    GuiSlotHover hover{};
    if (Widget* w = top->HitTest(m_cursorHotspot)) {
        if (auto* slot = dynamic_cast<Slot*>(w)) {
            hover.inventoryOwner = slot->GetInventoryOwner();
            hover.slotIndex = slot->GetSlotIndex();
        }
    }
    SetGuiSlotHover(m_registry, hover);

    if (m_input.IsActionPressed("interact_left")) {
        if (!top->OnClick(m_cursorHotspot, 0)) {
            m_eventBus.emit(events::DropCarriedStackPressedEvent{});
        }
    }
    if (m_input.IsActionPressed("interact_right")) {
        if (!top->OnClick(m_cursorHotspot, 1)) { /* consumed or not */ }
    }
}

} // namespace gui
