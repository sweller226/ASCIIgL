#pragma once

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/renderer/VertexShaderCPU.hpp>

class GuiManager {
public:
    GuiManager(unsigned int screenWidth = 600, unsigned int screenHeight = 400);
    ~GuiManager() = default;

    void Update(float deltaTime);
    void Render();

    const Camera2D& GetCamera() const { return guiCamera; }

private:
    Camera2D guiCamera;
};
