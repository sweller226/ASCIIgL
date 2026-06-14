#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace BlockTargetOutlineShaders {

const char* GetVSSource();
const char* GetPSSource();
ASCIIgL::UniformBufferLayout GetUniformLayout();

} // namespace BlockTargetOutlineShaders
