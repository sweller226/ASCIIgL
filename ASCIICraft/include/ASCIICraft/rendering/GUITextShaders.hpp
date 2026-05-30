#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace GUITextShaders {

    // Vertex shader - identical to texture array (passes position + UVLayer)
    const char* GetGUITextVSSource();
    
    // Pixel shader - samples texture, computes luminance, maps to gradient
    const char* GetGUITextPSSource();
    
    // Uniform layout including gradient parameters (MVP + gradient colors)
    ASCIIgL::UniformBufferLayout GetGUITextPSUniformLayout();

} // namespace GUITextShaders
