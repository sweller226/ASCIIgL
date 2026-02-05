#pragma once

#include <ASCIIgL/renderer/gpu/Shader.hpp>

// =========================================================================
// Shader Sources
// =========================================================================
// These shaders sample from a texture array and map the luminance to a 
// color gradient at full 24-bit precision. Quantization to palette happens
// later in the rendering pipeline.

namespace TerrainShaders {

    // Vertex shader - identical to texture array (passes position + UVLayer)
    const char* GetTerrainVSSource();
    
    // Pixel shader - samples texture, computes luminance, maps to gradient
    const char* GetTerrainPSSource();
    
    // Uniform layout including gradient parameters (MVP + gradient colors)
    ASCIIgL::UniformBufferLayout GetTerrainPSUniformLayout();

} // namespace TerrainShaders
