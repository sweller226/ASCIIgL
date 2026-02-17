#include <ASCIICraft/gui/GuiManager.hpp>
#include <ASCIICraft/gui/Screen.hpp>
#include <algorithm>
#include <glm/vec2.hpp>

namespace ASCIICraft::gui {

GuiManager::GuiManager(entt::registry& registry, EventBus& eventBus, ASCIICraft::IGameInputSource& input)
    : m_registry(registry)
    , m_eventBus(eventBus)
    , m_input(input)
{}

void GuiManager::SetScreenSize(glm::vec2 size) {
    m_screenSize = size;
    const auto w = static_cast<unsigned int>(size.x);
    const auto h = static_cast<unsigned int>(size.y);
    if (m_guiCamera)
        m_guiCamera->setScreenDimensions(w, h);
    else
        m_guiCamera = std::make_unique<ASCIIgL::Camera2D>(glm::vec2(0.0f, 0.0f), w, h);
}

void GuiManager::SetPlayScreen(std::unique_ptr<Screen> screen) {
    m_playScreen = std::move(screen);
    m_screenStack.clear();
    if (m_playScreen)
        m_screenStack.push_back(m_playScreen.get());
}

void GuiManager::SetInventoryScreen(std::unique_ptr<Screen> screen) {
    m_inventoryScreen = std::move(screen);
}

void GuiManager::ToggleInventoryScreen() {
    if (!m_inventoryScreen) return;
    auto it = std::find(m_screenStack.begin(), m_screenStack.end(), m_inventoryScreen.get());
    if (it != m_screenStack.end()) {
        m_screenStack.erase(it);
    } else {
        m_screenStack.push_back(m_inventoryScreen.get());
        m_cursorPosition = m_screenSize * 0.5f;
    }
}

void GuiManager::PushScreen(Screen* screen) {
    if (screen)
        m_screenStack.push_back(screen);
}

void GuiManager::PopScreen() {
    if (m_screenStack.size() > 1)
        m_screenStack.pop_back();
}

void GuiManager::Update() {
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    if (m_screenStack.empty()) return;

    for (Screen* screen : m_screenStack) {
        screen->Layout(m_screenSize, nullptr);
        screen->OnLayout(m_screenSize);
        screen->OnUpdate(dt);
    }

    Screen* top = m_screenStack.back();
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

void GuiManager::Draw(::ecs::systems::RenderSystem& renderSystem) {
    for (Screen* screen : m_screenStack)
        screen->OnDraw(renderSystem);
}

bool GuiManager::IsBlockingInput() const {
    if (m_screenStack.empty()) return false;
    return m_screenStack.back()->blocksInput;
}

bool GuiManager::IsInventoryOpen() const {
    if (!m_inventoryScreen) return false;
    return std::find(m_screenStack.begin(), m_screenStack.end(), m_inventoryScreen.get()) != m_screenStack.end();
}

} // namespace ASCIICraft::gui
