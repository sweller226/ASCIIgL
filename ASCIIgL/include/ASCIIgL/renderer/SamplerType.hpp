#pragma once

namespace ASCIIgL {

/// Sampler/filter choice for a texture slot.
/// Default = Renderer infers Point for both Texture and TextureArray.
enum class SamplerType {
    Default,    ///< Renderer chooses: Point for both Texture and TextureArray
    Point,      ///< MIN_MAG_MIP_POINT (pixel-art / cutouts; avoids opaque×transparent mip bleed)
    Anisotropic ///< Anisotropic filtering (optional 3D)
};

} // namespace ASCIIgL
