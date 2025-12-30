#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <any>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/Shader.hpp>

#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace ASCIIgL {

// Forward declarations
class Texture;
class RendererGPU;
class ShaderProgram;

// =========================================================================
// Texture Slot - Describes a texture binding point
// =========================================================================

struct TextureSlot {
    std::string name;           // Uniform name in shader (e.g., "diffuseTexture")
    uint32_t slot;              // Texture slot index (t0, t1, etc.)
    const Texture* texture;     // Currently bound texture (not owned)
    
    TextureSlot(const std::string& name, uint32_t slot)
        : name(name), slot(slot), texture(nullptr) {}
};

// =========================================================================
// Material - Holds shader + uniforms + textures (like Unity's Material)
// =========================================================================

class Material {
    friend class RendererGPU;

public:
    // Create a material from a shader program
    static std::unique_ptr<Material> Create(std::shared_ptr<ShaderProgram> program);
    
    // Create with default shader (convenience)
    static std::unique_ptr<Material> CreateDefault();

    ~Material();

    // =========================================================================
    // Uniform Setters (type-safe)
    // =========================================================================
    
    void SetFloat(const std::string& name, float value);
    void SetFloat2(const std::string& name, const glm::vec2& value);
    void SetFloat3(const std::string& name, const glm::vec3& value);
    void SetFloat4(const std::string& name, const glm::vec4& value);
    
    void SetInt(const std::string& name, int value);
    void SetInt2(const std::string& name, const glm::ivec2& value);
    void SetInt3(const std::string& name, const glm::ivec3& value);
    void SetInt4(const std::string& name, const glm::ivec4& value);
    
    void SetMatrix3(const std::string& name, const glm::mat3& value);
    void SetMatrix4(const std::string& name, const glm::mat4& value);
    
    // Generic setter (uses variant)
    void SetUniform(const std::string& name, const UniformValue& value);

    // =========================================================================
    // Uniform Getters
    // =========================================================================
    
    template<typename T>
    T GetUniform(const std::string& name) const {
        auto it = _uniformValues.find(name);
        if (it != _uniformValues.end()) {
            return std::get<T>(it->second);
        }
        return T{};
    }
    
    bool HasUniform(const std::string& name) const;

    // =========================================================================
    // Texture Binding
    // =========================================================================
    
    void SetTexture(const std::string& name, const Texture* texture);
    void SetTexture(uint32_t slot, const Texture* texture);
    const Texture* GetTexture(const std::string& name) const;
    const Texture* GetTexture(uint32_t slot) const;
    
    // Add a texture slot (for custom shaders with multiple textures)
    void AddTextureSlot(const std::string& name, uint32_t slot);

    // =========================================================================
    // Shader Program Access
    // =========================================================================
    
    std::shared_ptr<ShaderProgram> GetShaderProgram() const { return _program; }
    void SetShaderProgram(std::shared_ptr<ShaderProgram> program);

    // =========================================================================
    // State Management
    // =========================================================================
    
    bool IsDirty() const { return _uniformsDirty; }
    void ClearDirty() { _uniformsDirty = false; }

    // Clone this material (creates a copy with same shader but independent uniforms)
    std::unique_ptr<Material> Clone() const;

private:
    Material();

    void SetUniformInternal(const std::string& name, const UniformValue& value);
    void UpdateConstantBufferData();

    std::shared_ptr<ShaderProgram> _program;
    
    // Uniform storage
    std::unordered_map<std::string, UniformValue> _uniformValues;
    std::vector<std::byte> _constantBufferData;  // Packed data for GPU upload
    bool _uniformsDirty = true;
    
    // Texture slots
    std::vector<TextureSlot> _textureSlots;

#ifdef _WIN32
    Microsoft::WRL::ComPtr<ID3D11Buffer> _constantBuffer;
    bool _constantBufferInitialized = false;
#endif
};

// =========================================================================
// Material Library - Manages shared materials (optional singleton)
// =========================================================================

class MaterialLibrary {
public:
    static MaterialLibrary& GetInst() {
        static MaterialLibrary instance;
        return instance;
    }

    // Register a material with a name for reuse
    void Register(const std::string& name, std::shared_ptr<Material> material);
    
    // Get a registered material (returns nullptr if not found)
    std::shared_ptr<Material> Get(const std::string& name) const;
    
    // Check if material exists
    bool Has(const std::string& name) const;
    
    // Remove a material
    void Remove(const std::string& name);
    
    // Clear all materials
    void Clear();

    // Get default material (creates if needed)
    std::shared_ptr<Material> GetDefault();

private:
    MaterialLibrary() = default;
    ~MaterialLibrary() = default;
    MaterialLibrary(const MaterialLibrary&) = delete;
    MaterialLibrary& operator=(const MaterialLibrary&) = delete;

    std::unordered_map<std::string, std::shared_ptr<Material>> _materials;
    std::shared_ptr<Material> _defaultMaterial;
};

} // namespace ASCIIgL
