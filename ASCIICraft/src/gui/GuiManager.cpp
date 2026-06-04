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
        const entt::entity player = ecs::components::GetPlayerEntity(m_registry);
        if (player != entt::null && m_registry.valid(player)) {
            if (const auto* carried = m_registry.try_get<ecs::components::ItemCarried>(player)) {
                DrawItemStackIcon(m_registry, *m_renderer, carried->stack, m_cursorPosition, m_cursorSize, 9999);
            }
        }

        if (m_cursorSurface.mesh && m_cursorSurface.material) {
            m_renderer->RenderGUIQuad(
                m_cursorPosition, m_cursorSize, 10000, m_cursorSurface.mesh, m_cursorSurface.material);
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
    m_cursorSurface = GUISurface::FromTexture(cursorTexture);
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
        glm::vec2 initialCursor{};
        if (screen->TryGetInitialCursorPosition(screenSize, initialCursor)) {
            m_cursorPosition = initialCursor;
        }
    }
}

void GUIManager::UpdateCursor(GUIScreen* top) {
    if (!top || !top->blocksInput) return;

    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    const float cursorStep = m_cursorMoveSpeed * dt;

    if (m_input.IsActionHeld("camera_left"))  m_cursorPosition.x -= cursorStep;
    if (m_input.IsActionHeld("camera_right")) m_cursorPosition.x += cursorStep;
    if (m_input.IsActionHeld("camera_up"))    m_cursorPosition.y -= cursorStep;
    if (m_input.IsActionHeld("camera_down"))  m_cursorPosition.y += cursorStep;

    m_cursorPosition.x = std::clamp(m_cursorPosition.x, 0.0f, m_screenSize.x);
    m_cursorPosition.y = std::clamp(m_cursorPosition.y, 0.0f, m_screenSize.y);

    if (m_input.IsActionPressed("quit"))
        PopScreen();

    top->OnCursorMove(m_cursorPosition);

    GuiSlotHover hover{};
    if (Widget* w = top->HitTest(m_cursorPosition)) {
        if (auto* slot = dynamic_cast<Slot*>(w)) {
            hover.inventoryOwner = slot->GetInventoryOwner();
            hover.slotIndex = slot->GetSlotIndex();
        }
    }
    SetGuiSlotHover(m_registry, hover);

    if (m_input.IsActionPressed("interact_left")) {
        if (!top->OnClick(m_cursorPosition, 0)) {
            m_eventBus.emit(events::DropCarriedStackPressedEvent{});
        }
    }
    if (m_input.IsActionPressed("interact_right")) {
        if (!top->OnClick(m_cursorPosition, 1)) { /* consumed or not */ }
    }
}

} // namespace gui
