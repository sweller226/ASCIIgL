#pragma once

#include <ASCIIgL/engine/Camera2D.hpp>

class GuiManager {
public:
    GuiManager(unsigned int screenWidth = 600, unsigned int screenHeight = 400);
    ~GuiManager() = default;

    void Update(float deltaTime);
    void Render();

    const ASCIIgL::Camera2D& GetCamera() const { return guiCamera; }

private:
    ASCIIgL::Camera2D guiCamera;
};
