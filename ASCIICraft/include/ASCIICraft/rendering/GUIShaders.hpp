#pragma once

#include <ASCIIgL/renderer/gpu/Shader.hpp>

namespace GUIShaders {

// GUI

const char* GetGUIVSSource();
const char* GetGUIPSSource();

ASCIIgL::UniformBufferLayout GetGUIPSUniformLayout();

// Item

const char* GetItemVSSource();
const char* GetItemPSSource();

ASCIIgL::UniformBufferLayout GetItemPSUniformLayout();

} // namespace GUIShaders