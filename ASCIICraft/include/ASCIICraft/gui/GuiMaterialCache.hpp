#pragma once

#include <memory>
#include <unordered_map>

namespace ASCIIgL { class Texture; class Material; class ShaderProgram; }

namespace ASCIICraft::gui {

/// Caches GUI panel materials per texture. Creates one material instance per unique texture.
/// Materials are created from the shared "guiMaterial" shader program template.
class GuiMaterialCache {
public:
    static GuiMaterialCache& GetInst() {
        static GuiMaterialCache instance;
        return instance;
    }

    /// Get or create a material for the given texture. Returns nullptr if texture is null.
    /// Materials are cached by texture pointer, so same texture = same material.
    std::shared_ptr<ASCIIgL::Material> GetMaterialForTexture(std::shared_ptr<ASCIIgL::Texture> texture);

    /// Clear all cached materials (call on shutdown or when textures are invalidated)
    void Clear();

private:
    GuiMaterialCache() = default;
    ~GuiMaterialCache() = default;
    GuiMaterialCache(const GuiMaterialCache&) = delete;
    GuiMaterialCache& operator=(const GuiMaterialCache&) = delete;

    std::shared_ptr<ASCIIgL::ShaderProgram> GetOrCreateGuiShaderProgram();
    
    std::unordered_map<const ASCIIgL::Texture*, std::weak_ptr<ASCIIgL::Material>> m_materialCache;
    std::shared_ptr<ASCIIgL::ShaderProgram> m_guiShaderProgram;
};

} // namespace ASCIICraft::gui
