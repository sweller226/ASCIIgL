#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace ASCIIgL {

// =========================================================================
// Vertex Element Type and Semantic Enums
// =========================================================================

enum class VertexElementType {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    UByte4,              // 4 unsigned bytes (0-255)
    UByte4Normalized,    // 4 unsigned bytes normalized to 0.0-1.0 (for colors)
    Short2,              // 2 signed shorts
    Short2Normalized,    // 2 signed shorts normalized to -1.0 to 1.0
    Short4,              // 4 signed shorts
    Short4Normalized,    // 4 signed shorts normalized to -1.0 to 1.0
    UShort2,             // 2 unsigned shorts
    UShort2Normalized,   // 2 unsigned shorts normalized to 0.0-1.0
    UShort4,             // 4 unsigned shorts
    UShort4Normalized    // 4 unsigned shorts normalized to 0.0-1.0
};

enum class VertexElementSemantic {
    Position,
    Normal,
    Tangent,
    Bitangent,
    Color,
    TexCoord0,
    TexCoord1,
    TexCoord2,
    TexCoord3,
    BoneIndices,
    BoneWeights,
    Custom0,   // User-defined semantics
    Custom1,
    Custom2,
    Custom3
};

// =========================================================================
// Vertex Element - Describes a single component of a vertex
// =========================================================================

class VertexElement {
public:
    VertexElement(VertexElementSemantic semantic, VertexElementType type, 
                  uint32_t offset, uint32_t semanticIndex = 0);

    VertexElementSemantic GetSemantic() const { return _semantic; }
    VertexElementType GetType() const { return _type; }
    uint32_t GetOffset() const { return _offset; }
    uint32_t GetSemanticIndex() const { return _semanticIndex; }
    uint32_t GetSize() const;  // Size in bytes

    // Comparison for caching
    bool operator==(const VertexElement& other) const;
    bool operator!=(const VertexElement& other) const { return !(*this == other); }

private:
    VertexElementSemantic _semantic;
    VertexElementType _type;
    uint32_t _offset;
    uint32_t _semanticIndex;
};

// =========================================================================
// Vertex Format - Describes the complete layout of a vertex
// =========================================================================

class VertFormat {
public:
    // Builder for fluent API
    class Builder {
    public:
        Builder& Add(VertexElementSemantic semantic, VertexElementType type, uint32_t semanticIndex = 0);
        
        // Convenience methods for common types
        Builder& AddFloat(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddFloat2(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddFloat3(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddFloat4(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddInt(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddInt2(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddInt3(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddInt4(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        Builder& AddUByte4Normalized(VertexElementSemantic semantic, uint32_t semanticIndex = 0);
        
        VertFormat Build() const;

    private:
        std::vector<VertexElement> _elements;
        uint32_t _currentOffset = 0;
    };

    VertFormat() = default;

    const std::vector<VertexElement>& GetElements() const { return _elements; }
    uint32_t GetStride() const { return _stride; }
    bool IsEmpty() const { return _elements.empty(); }

    // Comparison for caching
    bool operator==(const VertFormat& other) const;
    bool operator!=(const VertFormat& other) const { return !(*this == other); }

    // Hash support for unordered_map
    size_t GetHash() const;

private:
    friend class Builder;
    
    std::vector<VertexElement> _elements;
    uint32_t _stride = 0;
};

// =========================================================================
// Helper Functions
// =========================================================================

// Get size in bytes for a vertex element type
uint32_t GetVertexElementTypeSize(VertexElementType type);

// Get semantic name as string (for DirectX input layout)
const char* GetSemanticName(VertexElementSemantic semantic);

// =========================================================================
// Predefined Common Formats
// =========================================================================

namespace VertFormats {
    // Position (XYZW) + TexCoord (UVW) - Current ASCIIgL VERTEX format (7 floats = 28 bytes)
    const VertFormat& PosWUVInvW();
    
    // Position (XYZ) + TexCoord (UV) - Standard textured mesh
    const VertFormat& PosUV();
    
    // Position (XYZ) + Normal (XYZ) + TexCoord (UV) - Standard 3D with lighting
    const VertFormat& PosNormUV();
    
    // Position (XYZ) + Color (RGBA normalized) - Colored vertices
    const VertFormat& PosColor();
}

// =========================================================================
// Predefined Vertex Structs
// =========================================================================

namespace VertStructs {

// PosWUVInvW vertex: XYZW + UVW (7 floats = 28 bytes) - Used by CPU renderer for perspective division

struct PosWUVInvW {
    float data[7]; // XYZW + UVW

    // Accessors
    float X() const { return data[0]; }
    float Y() const { return data[1]; }
    float Z() const { return data[2]; }
    float W() const { return data[3]; }
    float U() const { return data[4]; }
    float V() const { return data[5]; }
    float InvW() const { return data[6]; }

    glm::vec2 GetXY() const { return glm::vec2(data[0], data[1]); }
    glm::vec3 GetXYZ() const { return glm::vec3(data[0], data[1], data[2]); }
    glm::vec4 GetXYZW() const { return glm::vec4(data[0], data[1], data[2], data[3]); }
    glm::vec2 GetUV() const { return glm::vec2(data[4], data[5]); }

    // Mutators
    void SetXY(const glm::vec2 v) { data[0] = v.x; data[1] = v.y; }
    void SetXYZ(const glm::vec3 v) { data[0] = v.x; data[1] = v.y; data[2] = v.z; }
    void SetXYZW(const glm::vec4 v) { data[0] = v.x; data[1] = v.y; data[2] = v.z; data[3] = v.w; }
    void SetUV(const glm::vec2 v) { data[4] = v.x; data[5] = v.y; }
    void SetUVW(const glm::vec3 v) { data[4] = v.x; data[5] = v.y; data[6] = v.z; }
    void SetInvW(float v) { data[6] = v; }
};

// PosUV vertex: XYZ + UV (5 floats = 20 bytes)
struct PosUV {
    float data[5];

    // Accessors
    float X() const { return data[0]; }
    float Y() const { return data[1]; }
    float Z() const { return data[2]; }
    float U() const { return data[3]; }
    float V() const { return data[4]; }

    glm::vec2 GetXY() const { return glm::vec2(data[0], data[1]); }
    glm::vec3 GetXYZ() const { return glm::vec3(data[0], data[1], data[2]); }
    glm::vec2 GetUV() const { return glm::vec2(data[3], data[4]); }

    // Mutators
    void SetXY(const glm::vec2 v) { data[0] = v.x; data[1] = v.y; }
    void SetXYZ(const glm::vec3 v) { data[0] = v.x; data[1] = v.y; data[2] = v.z; }
    void SetUV(const glm::vec2 v) { data[3] = v.x; data[4] = v.y; }
};

} // namespace VertStructs

} // namespace ASCIIgL

// Hash specialization for std::unordered_map support
namespace std {
    template<>
    struct hash<ASCIIgL::VertFormat> {
        size_t operator()(const ASCIIgL::VertFormat& format) const {
            return format.GetHash();
        }
    };
}