#include <ASCIICraft/gui/GUISurface.hpp>

#include <ASCIICraft/util/QuadMeshBuilder.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/renderer/Material.hpp>

namespace gui {

GUISurface GUISurface::FromAtlasRegion(const std::shared_ptr<ASCIIgL::Texture>& texture,
                                       float atlasSize,
                                       int x, int y, int w, int h,
                                       const std::string& materialName) {
    GUISurface surface;
    surface.mesh = util::QuadMeshBuilder::BuildPosUVQuad(texture, util::PixelRectToUV(atlasSize, x, y, w, h));
    surface.material = ASCIIgL::MaterialLibrary::GetInst().GetOrCreateFromTemplate("guiMaterial", materialName);
    if (surface.material && texture) {
        surface.material->SetTexture("diffuseTexture", texture.get());
    }
    return surface;
}

GUISurface GUISurface::FromTexture(const std::shared_ptr<ASCIIgL::Texture>& texture,
                                   const std::string& materialName) {
    GUISurface surface;
    // Full texture in the same (minU, topV, maxU, bottomV) convention.
    surface.mesh = util::QuadMeshBuilder::BuildPosUVQuad(texture, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    surface.material = ASCIIgL::MaterialLibrary::GetInst().GetOrCreateFromTemplate("guiMaterial", materialName);
    if (surface.material && texture) {
        surface.material->SetTexture("diffuseTexture", texture.get());
    }
    return surface;
}

} // namespace gui
