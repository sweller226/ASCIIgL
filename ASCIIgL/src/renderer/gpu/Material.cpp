#include <ASCIIgL/renderer/gpu/Material.hpp>

#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <cstring>

namespace ASCIIgL {

// =========================================================================
// Material Implementation
// =========================================================================

Material::Material() = default;
Material::~Material() = default;

std::unique_ptr<Material> Material::Create(std::shared_ptr<ShaderProgram> program) {
    auto material = std::unique_ptr<Material>(new Material());
    material->_program = program;
    
    if (program && program->IsValid()) {
        // Initialize constant buffer data with the correct size
        const auto& layout = program->GetUniformLayout();
        material->_constantBufferData.resize(layout.GetSize(), std::byte{0});
        
        // Add default texture slot for diffuse texture
        material->AddTextureSlot("diffuseTexture", 0);
    }
    
    return material;
}

std::unique_ptr<Material> Material::CreateDefault() {
    // Create default shaders
    auto vs = Shader::CreateFromSource(
        DefaultShaders::GetDefaultVertexShaderSource(),
        ShaderType::Vertex
    );
    
    auto ps = Shader::CreateFromSource(
        DefaultShaders::GetDefaultPixelShaderSource(),
        ShaderType::Pixel
    );
    
    // Create shader program
    auto program = ShaderProgram::Create(
        std::move(vs),
        std::move(ps),
        VertFormats::PosUV(),
        DefaultShaders::GetDefaultUniformLayout()
    );
    
    if (!program || !program->IsValid()) {
        Logger::Error("Failed to create default material shader program");
        return nullptr;
    }
    
    // Convert unique_ptr to shared_ptr
    std::shared_ptr<ShaderProgram> sharedProgram = std::move(program);
    return Create(sharedProgram);
}

// =========================================================================
// Uniform Setters
// =========================================================================

void Material::SetFloat(const std::string& name, float value) {
    SetUniformInternal(name, value);
}

void Material::SetFloat2(const std::string& name, const glm::vec2& value) {
    SetUniformInternal(name, value);
}

void Material::SetFloat3(const std::string& name, const glm::vec3& value) {
    SetUniformInternal(name, value);
}

void Material::SetFloat4(const std::string& name, const glm::vec4& value) {
    SetUniformInternal(name, value);
}

void Material::SetInt(const std::string& name, int value) {
    SetUniformInternal(name, value);
}

void Material::SetInt2(const std::string& name, const glm::ivec2& value) {
    SetUniformInternal(name, value);
}

void Material::SetInt3(const std::string& name, const glm::ivec3& value) {
    SetUniformInternal(name, value);
}

void Material::SetInt4(const std::string& name, const glm::ivec4& value) {
    SetUniformInternal(name, value);
}

void Material::SetMatrix3(const std::string& name, const glm::mat3& value) {
    SetUniformInternal(name, value);
}

void Material::SetMatrix4(const std::string& name, const glm::mat4& value) {
    SetUniformInternal(name, value);
}

void Material::SetUniform(const std::string& name, const UniformValue& value) {
    SetUniformInternal(name, value);
}

void Material::SetUniformInternal(const std::string& name, const UniformValue& value) {
    _uniformValues[name] = value;
    _uniformsDirty = true;
}

bool Material::HasUniform(const std::string& name) const {
    return _uniformValues.find(name) != _uniformValues.end();
}

// =========================================================================
// Constant Buffer Data Packing
// =========================================================================

void Material::UpdateConstantBufferData() {
    if (!_program || !_uniformsDirty) return;
    
    const auto& layout = _program->GetUniformLayout();
    
    for (const auto& [name, value] : _uniformValues) {
        const UniformDescriptor* desc = layout.GetUniform(name);
        if (!desc) {
            Logger::Warning("Uniform '" + name + "' not found in shader layout");
            continue;
        }
        
        // Copy value to constant buffer data at the correct offset
        std::visit([this, desc](auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if (desc->offset + sizeof(T) <= _constantBufferData.size()) {
                std::memcpy(_constantBufferData.data() + desc->offset, &val, sizeof(T));
            }
        }, value);
    }
    
    _uniformsDirty = false;
}

// =========================================================================
// Texture Binding
// =========================================================================

void Material::SetTexture(const std::string& name, const Texture* texture) {
    for (auto& slot : _textureSlots) {
        if (slot.name == name) {
            slot.texture = texture;
            return;
        }
    }
    
    Logger::Warning("Texture slot '" + name + "' not found in material");
}

void Material::SetTexture(uint32_t slot, const Texture* texture) {
    for (auto& texSlot : _textureSlots) {
        if (texSlot.slot == slot) {
            texSlot.texture = texture;
            return;
        }
    }
    
    // Add a new slot if not found
    _textureSlots.emplace_back("texture" + std::to_string(slot), slot);
    _textureSlots.back().texture = texture;
}

const Texture* Material::GetTexture(const std::string& name) const {
    for (const auto& slot : _textureSlots) {
        if (slot.name == name) {
            return slot.texture;
        }
    }
    return nullptr;
}

const Texture* Material::GetTexture(uint32_t slot) const {
    for (const auto& texSlot : _textureSlots) {
        if (texSlot.slot == slot) {
            return texSlot.texture;
        }
    }
    return nullptr;
}

void Material::AddTextureSlot(const std::string& name, uint32_t slot) {
    // Check if slot already exists
    for (const auto& texSlot : _textureSlots) {
        if (texSlot.name == name || texSlot.slot == slot) {
            return;  // Already exists
        }
    }
    _textureSlots.emplace_back(name, slot);
}

// =========================================================================
// Shader Program Management
// =========================================================================

void Material::SetShaderProgram(std::shared_ptr<ShaderProgram> program) {
    _program = program;
    
    if (program && program->IsValid()) {
        // Resize constant buffer data
        const auto& layout = program->GetUniformLayout();
        _constantBufferData.resize(layout.GetSize(), std::byte{0});
    }
    
    _uniformsDirty = true;
    _constantBufferInitialized = false;
}

// =========================================================================
// Clone
// =========================================================================

std::unique_ptr<Material> Material::Clone() const {
    auto clone = std::unique_ptr<Material>(new Material());
    clone->_program = _program;  // Share the shader program
    clone->_uniformValues = _uniformValues;
    clone->_constantBufferData = _constantBufferData;
    clone->_uniformsDirty = true;
    clone->_textureSlots = _textureSlots;
    // Don't copy GPU resources - let them be recreated
    return clone;
}

// =========================================================================
// MaterialLibrary Implementation
// =========================================================================

void MaterialLibrary::Register(const std::string& name, std::shared_ptr<Material> material) {
    _materials[name] = material;
}

std::shared_ptr<Material> MaterialLibrary::Get(const std::string& name) const {
    auto it = _materials.find(name);
    if (it != _materials.end()) {
        return it->second;
    }
    return nullptr;
}

bool MaterialLibrary::Has(const std::string& name) const {
    return _materials.find(name) != _materials.end();
}

void MaterialLibrary::Remove(const std::string& name) {
    _materials.erase(name);
}

void MaterialLibrary::Clear() {
    _materials.clear();
    _defaultMaterial.reset();
}

std::shared_ptr<Material> MaterialLibrary::GetDefault() {
    if (!_defaultMaterial) {
        auto mat = Material::CreateDefault();
        if (mat) {
            _defaultMaterial = std::shared_ptr<Material>(mat.release());
        }
    }
    return _defaultMaterial;
}

} // namespace ASCIIgL
