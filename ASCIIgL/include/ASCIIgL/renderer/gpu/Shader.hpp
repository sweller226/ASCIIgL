#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <variant>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

#ifdef _WIN32
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#endif

namespace ASCIIgL {

// Forward declarations
class Texture;
class RendererGPU;

// =========================================================================
// Shader Type Enum
// =========================================================================

enum class ShaderType {
    Vertex,
    Pixel
};

// =========================================================================
// Uniform Types - Values that can be passed to shaders
// =========================================================================

// Supported uniform value types (variant for type-safe storage)
using UniformValue = std::variant<
    float,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    int,
    glm::ivec2,
    glm::ivec3,
    glm::ivec4,
    glm::mat3,
    glm::mat4
>;

// Uniform type enum for serialization/reflection
enum class UniformType {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    Mat3,
    Mat4
};

// =========================================================================
// Uniform Descriptor - Describes a single uniform in a constant buffer
// =========================================================================

struct UniformDescriptor {
    std::string name;
    UniformType type;
    uint32_t offset;      // Byte offset in constant buffer
    uint32_t size;        // Size in bytes

    UniformDescriptor(const std::string& name, UniformType type, uint32_t offset);
    
    static uint32_t GetTypeSize(UniformType type);
    static uint32_t GetTypeAlignment(UniformType type);  // For HLSL packing rules
};

// =========================================================================
// Uniform Buffer Layout - Describes the layout of a constant buffer
// =========================================================================

class UniformBufferLayout {
public:
    class Builder {
    public:
        Builder& Add(const std::string& name, UniformType type);
        UniformBufferLayout Build() const;

    private:
        std::vector<UniformDescriptor> _uniforms;
        uint32_t _currentOffset = 0;
    };

    const std::vector<UniformDescriptor>& GetUniforms() const { return _uniforms; }
    uint32_t GetSize() const { return _size; }
    bool HasUniform(const std::string& name) const;
    const UniformDescriptor* GetUniform(const std::string& name) const;

private:
    friend class Builder;
    std::vector<UniformDescriptor> _uniforms;
    uint32_t _size = 0;
};

// =========================================================================
// Shader - Represents a single compiled shader (vertex or pixel)
// =========================================================================

class Shader {
    friend class ShaderProgram;
    friend class RendererGPU;

public:
    // Create from HLSL source code
    static std::unique_ptr<Shader> CreateFromSource(
        const std::string& source,
        ShaderType type,
        const std::string& entryPoint = "main"
    );

    // Create from precompiled bytecode (for faster loading)
    static std::unique_ptr<Shader> CreateFromBytecode(
        const std::vector<uint8_t>& bytecode,
        ShaderType type
    );

    ~Shader();

    ShaderType GetType() const { return _type; }
    bool IsValid() const { return _isValid; }
    const std::string& GetCompileError() const { return _compileError; }

private:
    Shader(ShaderType type);

    bool CompileFromSource(const std::string& source, const std::string& entryPoint);
    bool LoadFromBytecode(const std::vector<uint8_t>& bytecode);

    ShaderType _type;
    bool _isValid = false;
    std::string _compileError;

#ifdef _WIN32
    Microsoft::WRL::ComPtr<ID3DBlob> _bytecode;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader;
#endif
};

// =========================================================================
// ShaderProgram - Combines vertex + pixel shader with input layout
// =========================================================================

class ShaderProgram {
    friend class Material;
    friend class RendererGPU;

public:
    // Create a shader program from vertex and pixel shaders
    static std::unique_ptr<ShaderProgram> Create(
        std::unique_ptr<Shader> vertexShader,
        std::unique_ptr<Shader> pixelShader,
        const VertFormat& vertexFormat,
        const UniformBufferLayout& uniformLayout = UniformBufferLayout()
    );

    ~ShaderProgram();

    bool IsValid() const { return _isValid; }
    const VertFormat& GetVertexFormat() const { return _vertexFormat; }
    const UniformBufferLayout& GetUniformLayout() const { return _uniformLayout; }
    const std::string& GetError() const { return _error; }

private:
    ShaderProgram();

    bool Initialize(
        std::unique_ptr<Shader> vertexShader,
        std::unique_ptr<Shader> pixelShader,
        const VertFormat& vertexFormat,
        const UniformBufferLayout& uniformLayout
    );

    bool CreateInputLayout(const VertFormat& format);

    std::unique_ptr<Shader> _vertexShader;
    std::unique_ptr<Shader> _pixelShader;
    VertFormat _vertexFormat;
    UniformBufferLayout _uniformLayout;
    bool _isValid = false;
    std::string _error;

#ifdef _WIN32
    Microsoft::WRL::ComPtr<ID3D11InputLayout> _inputLayout;
#endif
};

// =========================================================================
// Default Shader Sources (provided by library)
// =========================================================================

namespace DefaultShaders {
    // Standard PosUV shader (MVP transform + texture sampling)
    const char* GetDefaultVertexShaderSource();
    const char* GetDefaultPixelShaderSource();
    
    // Unlit color shader (for solid colors without textures)
    const char* GetUnlitColorVertexShaderSource();
    const char* GetUnlitColorPixelShaderSource();
    
    // Get the default uniform layout (just MVP matrix)
    UniformBufferLayout GetDefaultUniformLayout();
}

} // namespace ASCIIgL
