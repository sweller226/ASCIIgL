#include <ASCIICraft/gui/GuiManager.hpp>

GuiManager::GuiManager(unsigned int screenWidth, unsigned int screenHeight) 
    : guiCamera(glm::vec2(0.0f, 0.0f), screenWidth, screenHeight) {
    // Initialize GUI camera for 2D rendering at origin
    // Default screen dimensions can be updated later
}

void GuiManager::Update(float deltaTime) {
    // Update GUI camera matrices
    guiCamera.recalculateViewMat();
    
    // TODO: Update GUI elements, animations, etc.
}

void GuiManager::Render() {
    // TODO: Render GUI elements using the GUI camera
    // This would typically involve drawing UI components like:
    // - HUD elements
    // - Inventory screens
    // - Menus
    // - Debug information
}
