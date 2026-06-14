#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace ASCIIgL {

/// HLSL include files for use with Shader::CreateFromSource(..., pIncludes).
/// Use #include "ColorUtil.hlsl" in shaders and pass the map from AddToMap().
namespace HLSLIncludes {

/// Filename used in #include "ColorUtil.hlsl"
const char* ColorUtilFileName();

/// HLSL source for ColorUtil.hlsl (sRGB/linear + luminance-based gradient).
const char* ColorUtilSource();

/// Add ASCIIgL HLSL includes to the given map (e.g. ColorUtil.hlsl).
void AddToMap(ShaderIncludeMap& map);

} // namespace HLSLIncludes
} // namespace ASCIIgL
