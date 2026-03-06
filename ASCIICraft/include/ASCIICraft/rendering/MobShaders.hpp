#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace MobShaders {

// Vertex shader - PosUV format (position + UV, no texture layer)
const char* GetMobVSSource();

// Pixel shader - samples Texture2D, gradient maps luminance, applies fog
const char* GetMobPSSource();

// Uniform layout (MVP + gradient + fog)
ASCIIgL::UniformBufferLayout GetMobPSUniformLayout();

} // namespace MobShaders
