#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

namespace ASCIIgL {

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

struct UniformDescriptor {
    std::string name;
    UniformType type;
    uint32_t offset;
    uint32_t size;

    UniformDescriptor(const std::string& name, UniformType type, uint32_t offset);

    static uint32_t GetTypeSize(UniformType type);
    static uint32_t GetTypeAlignment(UniformType type);
};

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

} // namespace ASCIIgL
