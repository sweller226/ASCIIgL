#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace HeldItemShaders {

const char* GetVSSource();
const char* GetPSSource();
ASCIIgL::UniformBufferLayout GetUniformLayout();

} // namespace HeldItemShaders
