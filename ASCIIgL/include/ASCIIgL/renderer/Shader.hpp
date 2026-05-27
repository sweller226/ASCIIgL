#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <variant>

#include <ASCIIgL/renderer/UniformLayout.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

// Forward declarations
class Texture;
class Renderer;

enum class ShaderType {
    Vertex,
    Pixel
};

using ShaderIncludeMap = std::unordered_map<std::string, std::string>;

// =========================================================================
// Shader - Represents a single compiled shader (vertex or pixel)
// =========================================================================

class Shader {
    friend class ShaderProgram;
    friend class Renderer;

public:
    // Create from HLSL source code (no includes)
    static std::unique_ptr<Shader> CreateFromSource(
        const std::string& source,
        ShaderType type,
        const std::string& entryPoint = "main"
    );

    // Create from HLSL source with shared includes (for #include "Util.hlsl" etc.)
    static std::unique_ptr<Shader> CreateFromSource(
        const std::string& source,
        ShaderType type,
        const std::string& entryPoint,
        const ShaderIncludeMap* pIncludes
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
    class Impl;
    Shader(ShaderType type);

    bool CompileFromSource(const std::string& source, const std::string& entryPoint,
                          const ShaderIncludeMap* pIncludes = nullptr);
    bool LoadFromBytecode(const std::vector<uint8_t>& bytecode);

    ShaderType _type;
    bool _isValid = false;
    std::string _compileError;
    std::unique_ptr<Impl> _impl;
};

// =========================================================================
// ShaderProgram - Combines vertex + pixel shader with input layout
// =========================================================================

class ShaderProgram {
    friend class Material;
    friend class Renderer;

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
    class Impl;
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
    std::unique_ptr<Impl> _impl;
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
    
    // Texture array shader (for Texture2DArray - PosUVLayer format)
    const char* GetTextureArrayVertexShaderSource();
    const char* GetTextureArrayPixelShaderSource();
    
    // Get the default uniform layout (just MVP matrix)
    UniformBufferLayout GetDefaultUniformLayout();
}

} // namespace ASCIIgL
