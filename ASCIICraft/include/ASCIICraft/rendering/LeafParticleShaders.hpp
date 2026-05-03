// ASCIICraft/rendering/LeafParticleShaders.hpp
#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace LeafParticleShaders {
    const char* GetVSSource();
    const char* GetPSSource();
    ASCIIgL::UniformBufferLayout GetUniformLayout();
}