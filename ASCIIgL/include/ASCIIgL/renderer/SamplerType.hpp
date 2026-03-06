#pragma once

namespace ASCIIgL {

/// Sampler/filter choice for a texture slot.
/// Default = Renderer infers (Texture → Point, TextureArray → Anisotropic).
enum class SamplerType {
    Default,    ///< Renderer chooses: Point for both Texture and TextureArray
    Point,      ///< Point + linear mip (pixel-art, GUI)
    Anisotropic ///< Anisotropic filtering (terrain, 3D)
};

} // namespace ASCIIgL
