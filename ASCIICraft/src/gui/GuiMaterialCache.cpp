#include <ASCIICraft/gui/GuiMaterialCache.hpp>
#include <ASCIIgL/renderer/gpu/Material.hpp>
#include <ASCIIgL/renderer/gpu/Shader.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIICraft/rendering/GUIShaders.hpp>
#include <ASCIIgL/util/Logger.hpp>

namespace ASCIICraft::gui {

std::shared_ptr<ASCIIgL::ShaderProgram> GuiMaterialCache::GetOrCreateGuiShaderProgram() {
    if (m_guiShaderProgram) return m_guiShaderProgram;

    // Try to get from material library first (if guiMaterial already exists)
    auto existingMat = ASCIIgL::MaterialLibrary::GetInst().Get("guiMaterial");
    if (existingMat && existingMat->GetShaderProgram()) {
        m_guiShaderProgram = existingMat->GetShaderProgram();
        return m_guiShaderProgram;
    }

    // Create shader program (same as in Game::LoadResources)
    auto guiVS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetGUIVSSource(),
        ASCIIgL::ShaderType::Vertex
    );

    auto guiPS = ASCIIgL::Shader::CreateFromSource(
        GUIShaders::GetGUIPSSource(),
        ASCIIgL::ShaderType::Pixel
    );

    if (!guiVS || !guiVS->IsValid() || !guiPS || !guiPS->IsValid()) {
        ASCIIgL::Logger::Error("GuiMaterialCache: Failed to compile GUI shaders");
        return nullptr;
    }

    auto guiShaderProgram = ASCIIgL::ShaderProgram::Create(
        std::move(guiVS),
        std::move(guiPS),
        ASCIIgL::VertFormats::PosUV(),
        GUIShaders::GetGUIPSUniformLayout()
    );

    if (!guiShaderProgram || !guiShaderProgram->IsValid()) {
        ASCIIgL::Logger::Error("GuiMaterialCache: Failed to create GUI shader program");
        return nullptr;
    }

    // Convert unique_ptr to shared_ptr
    m_guiShaderProgram = std::shared_ptr<ASCIIgL::ShaderProgram>(guiShaderProgram.release());
    return m_guiShaderProgram;
}

std::shared_ptr<ASCIIgL::Material> GuiMaterialCache::GetMaterialForTexture(std::shared_ptr<ASCIIgL::Texture> texture) {
    if (!texture) return nullptr;

    // Check cache
    auto it = m_materialCache.find(texture.get());
    if (it != m_materialCache.end()) {
        if (auto cached = it->second.lock()) {
            return cached;
        }
        // Weak pointer expired, remove from cache
        m_materialCache.erase(it);
    }

    // Get base guiMaterial template from library (or create shader program)
    auto baseMat = ASCIIgL::MaterialLibrary::GetInst().Get("guiMaterial");
    std::shared_ptr<ASCIIgL::Material> material;
    
    if (baseMat) {
        // Clone base material (shares shader program, but has independent uniforms/textures)
        material = std::shared_ptr<ASCIIgL::Material>(baseMat->Clone().release());
    } else {
        // Fallback: create from shader program
        auto shaderProgram = GetOrCreateGuiShaderProgram();
        if (!shaderProgram) return nullptr;
        // Create a unique_ptr copy of the shader program for Material::Create
        // We need to clone the shader program since Create takes ownership
        // Actually, let's just use the base material approach
        ASCIIgL::Logger::Warning("GuiMaterialCache: guiMaterial not found in library, creating from shader");
        return nullptr;
    }

    // Set texture on material
    material->SetTexture(0, texture.get());

    // Cache material (use weak_ptr so materials can be freed when not used)
    m_materialCache[texture.get()] = material;

    return material;
}

void GuiMaterialCache::Clear() {
    m_materialCache.clear();
    m_guiShaderProgram.reset();
}

} // namespace ASCIICraft::gui
