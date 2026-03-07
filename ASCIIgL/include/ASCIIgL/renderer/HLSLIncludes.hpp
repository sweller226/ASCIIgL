#pragma once

#include <ASCIIgL/renderer/Shader.hpp>

namespace ASCIIgL {

/// HLSL include files for use with Shader::CreateFromSource(..., pIncludes).
/// Use #include "ColorMonochrome.hlsl" in shaders and pass the map from AddToMap().
namespace HLSLIncludes {

/// Filename used in #include "ColorMonochrome.hlsl"
const char* ColorMonochromeFileName();

/// HLSL source for ColorMonochrome.hlsl (sRGB/linear + luminance-based gradient).
const char* ColorMonochromeSource();

/// Add ASCIIgL HLSL includes to the given map (e.g. ColorMonochrome.hlsl).
void AddToMap(ShaderIncludeMap& map);

} // namespace HLSLIncludes
} // namespace ASCIIgL
