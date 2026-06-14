#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace DroppedItemShaders {

const char* GetVSSource();
const char* GetPSSource();
ASCIIgL::UniformBufferLayout GetUniformLayout();

} // namespace DroppedItemShaders
