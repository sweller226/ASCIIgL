#include <ASCIIgL/renderer/Material.hpp>

#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <cstring>

namespace ASCIIgL {

// =========================================================================
// Material Implementation
// =========================================================================

Material::Material() = default;
Material::~Material() = default;

std::unique_ptr<Material> Material::Create(std::unique_ptr<ShaderProgram> program) {
    auto material = std::unique_ptr<Material>(new Material());
    material->_program = std::move(program);
    
    if (material->_program && material->_program->IsValid()) {
        // Initialize constant buffer data with the correct size
        const auto& layout = material->_program->GetUniformLayout();
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
    
    return Create(std::move(program));
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

const UniformDescriptor* Material::GetUniformDescriptor(const std::string& name) const {
    if (!_program || !_program->IsValid()) {
        return nullptr;
    }
    const auto& layout = _program->GetUniformLayout();
    return layout.GetUniform(name);
}

void Material::ApplyUniformOverride(const UniformDescriptor& desc, const UniformValue& value) {
    if (_constantBufferData.empty()) {
        return;
    }

    std::visit([this, &desc](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if (desc.offset + sizeof(T) <= _constantBufferData.size()) {
            std::memcpy(_constantBufferData.data() + desc.offset, &val, sizeof(T));
        }
    }, value);
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

void Material::SetTextureArray(const std::string& name, const TextureArray* textureArray) {
    for (auto& slot : _textureSlots) {
        if (slot.name == name) {
            slot.textureArray = textureArray;
            return;
        }
    }
    
    // Warn if not found (or should we add it? typically slots match shader registers)
    Logger::Warning("Texture slot '" + name + "' not found in material");
}

void Material::SetTextureArray(uint32_t slot, const TextureArray* textureArray) {
    for (auto& texSlot : _textureSlots) {
        if (texSlot.slot == slot) {
            texSlot.textureArray = textureArray;
            return;
        }
    }

    // Add a new slot if not found
    _textureSlots.emplace_back("texture" + std::to_string(slot), slot);
    _textureSlots.back().textureArray = textureArray;
}

const TextureArray* Material::GetTextureArray(const std::string& name) const {
    for (const auto& slot : _textureSlots) {
        if (slot.name == name) {
            return slot.textureArray;
        }
    }
    return nullptr;
}

const TextureArray* Material::GetTextureArray(uint32_t slot) const {
    for (const auto& texSlot : _textureSlots) {
        if (texSlot.slot == slot) {
            return texSlot.textureArray;
        }
    }
    return nullptr;
}

void Material::SetSamplerForSlot(const std::string& name, SamplerType type) {
    for (auto& slot : _textureSlots) {
        if (slot.name == name) {
            slot.samplerType = type;
            return;
        }
    }
    Logger::Warning("SetSamplerForSlot: texture slot '" + name + "' not found");
}

void Material::SetSamplerForSlot(uint32_t slot, SamplerType type) {
    for (auto& texSlot : _textureSlots) {
        if (texSlot.slot == slot) {
            texSlot.samplerType = type;
            return;
        }
    }
    Logger::Warning("SetSamplerForSlot: texture slot " + std::to_string(slot) + " not found");
}

SamplerType Material::GetSamplerForSlot(uint32_t slot) const {
    for (const auto& texSlot : _textureSlots) {
        if (texSlot.slot == slot) {
            return texSlot.samplerType;
        }
    }
    return SamplerType::Default;
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

std::shared_ptr<Material> MaterialLibrary::GetOrCreateFromTemplate(const std::string& templateName,
    const Texture* texture, uint32_t textureSlot) {
    if (!texture) return nullptr;

    const std::string key = templateName + "@" + std::to_string(reinterpret_cast<uintptr_t>(texture));
    auto it = _templateTextureCache.find(key);
    if (it != _templateTextureCache.end()) {
        if (auto cached = it->second.lock())
            return cached;
        _templateTextureCache.erase(it);
    }

    auto base = Get(templateName);
    if (!base) return nullptr;

    std::shared_ptr<Material> material(base->Clone().release());
    material->SetTexture(textureSlot, texture);
    _templateTextureCache[key] = material;
    return material;
}

bool MaterialLibrary::Has(const std::string& name) const {
    return _materials.find(name) != _materials.end();
}

void MaterialLibrary::Remove(const std::string& name) {
    _materials.erase(name);
}

void MaterialLibrary::Clear() {
    _materials.clear();
    _templateTextureCache.clear();
}

std::shared_ptr<Material> MaterialLibrary::GetDefault() {
    auto it = _materials.find(_defaultMaterialName);
    if (it != _materials.end() && it->second)
        return it->second;
    auto mat = Material::CreateDefault();
    if (!mat) return nullptr;
    std::shared_ptr<Material> stored(mat.release());
    _materials[_defaultMaterialName] = stored;
    return stored;
}

} // namespace ASCIIgL
