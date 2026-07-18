#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace BreakOverlayShaders {

const char* GetVSSource();
const char* GetPSSource();
ASCIIgL::UniformBufferLayout GetUniformLayout();

} // namespace BreakOverlayShaders
