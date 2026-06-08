#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace GUIBlockShaders {

const char* GetBlockVSSource();
const char* GetBlockPSSource();

ASCIIgL::UniformBufferLayout GetBlockPSUniformLayout();

} // namespace GUIBlockShaders
