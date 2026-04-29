#include <ASCIICraft/gui/GUIManager.hpp>
#include <ASCIICraft/gui/GUIScreen.hpp>
#include <algorithm>
#include <glm/vec2.hpp>

namespace gui {

GUIManager::GUIManager(entt::registry& registry, ASCIIgL::EventBus& eventBus, IInputSource& input)
    : m_registry(registry)
    , m_eventBus(eventBus)
    , m_input(input)
{}

void GUIManager::SetScreenSize(glm::vec2 size) {
    m_screenSize = size;
    const auto w = static_cast<unsigned int>(size.x);
    const auto h = static_cast<unsigned int>(size.y);
    if (m_guiCamera)
        m_guiCamera->setScreenDimensions(w, h);
    else
        m_guiCamera = std::make_unique<ASCIIgL::Camera2D>(glm::vec2(0.0f, 0.0f), w, h);
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
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    if (m_screenStack.empty()) return;

    for (GUIScreen* screen : m_screenStack) {
        screen->Layout(m_screenSize, nullptr);
        screen->OnLayout(m_screenSize);
        screen->OnUpdate(dt);
    }

    GUIScreen* top = m_screenStack.back();
    if (top->blocksInput) {
        if (m_input.IsActionHeld("camera_left"))  m_cursorPosition.x -= m_cursorMoveSpeed;
        if (m_input.IsActionHeld("camera_right")) m_cursorPosition.x += m_cursorMoveSpeed;
        if (m_input.IsActionHeld("camera_up"))    m_cursorPosition.y -= m_cursorMoveSpeed;
        if (m_input.IsActionHeld("camera_down")) m_cursorPosition.y += m_cursorMoveSpeed;

        m_cursorPosition.x = std::clamp(m_cursorPosition.x, 0.0f, m_screenSize.x);
        m_cursorPosition.y = std::clamp(m_cursorPosition.y, 0.0f, m_screenSize.y);

        if (m_input.IsActionPressed("quit"))
            PopScreen();

        top->OnCursorMove(m_cursorPosition);

        if (m_input.IsActionPressed("interact_left")) {
            if (!top->OnClick(m_cursorPosition, 0)) { /* consumed or not */ }
        }
        if (m_input.IsActionPressed("interact_right")) {
            if (!top->OnClick(m_cursorPosition, 1)) { /* consumed or not */ }
        }
    }
}

void GUIManager::Draw(::ecs::systems::RenderSystem& renderSystem) {
    for (GUIScreen* screen : m_screenStack)
        screen->OnDraw(renderSystem);
}

bool GUIManager::IsBlockingInput() const {
    if (m_screenStack.empty()) return false;
    return m_screenStack.back()->blocksInput;
}

} // namespace gui
