#include <ASCIIgL/renderer/VertFormat.hpp>

#include <glm/glm.hpp>

#ifdef _WIN32
#include <d3d11.h>
#endif

#include <functional>

namespace ASCIIgL {

// =========================================================================
// VertexElement Implementation
// =========================================================================

VertexElement::VertexElement(VertexElementSemantic semantic, VertexElementType type, 
                             uint32_t offset, uint32_t semanticIndex)
    : _semantic(semantic)
    , _type(type)
    , _offset(offset)
    , _semanticIndex(semanticIndex)
{
}

uint32_t VertexElement::GetSize() const {
    return GetVertexElementTypeSize(_type);
}

bool VertexElement::operator==(const VertexElement& other) const {
    return _semantic == other._semantic &&
           _type == other._type &&
           _offset == other._offset &&
           _semanticIndex == other._semanticIndex;
}

// =========================================================================
// VertFormat::Builder Implementation
// =========================================================================

VertFormat::Builder& VertFormat::Builder::Add(VertexElementSemantic semantic, 
                                                   VertexElementType type, 
                                                   uint32_t semanticIndex) {
    _elements.emplace_back(semantic, type, _currentOffset, semanticIndex);
    _currentOffset += GetVertexElementTypeSize(type);
    return *this;
}

VertFormat::Builder& VertFormat::Builder::AddFloat(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Float, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddFloat2(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Float2, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddFloat3(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Float3, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddFloat4(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Float4, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddInt(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Int, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddInt2(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Int2, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddInt3(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Int3, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddInt4(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::Int4, semanticIndex);
}

VertFormat::Builder& VertFormat::Builder::AddUByte4Normalized(VertexElementSemantic semantic, uint32_t semanticIndex) {
    return Add(semantic, VertexElementType::UByte4Normalized, semanticIndex);
}

VertFormat VertFormat::Builder::Build() const {
    VertFormat format;
    format._elements = _elements;
    format._stride = _currentOffset;
    return format;
}

// =========================================================================
// VertFormat Implementation
// =========================================================================

bool VertFormat::operator==(const VertFormat& other) const {
    if (_stride != other._stride) return false;
    if (_elements.size() != other._elements.size()) return false;
    
    for (size_t i = 0; i < _elements.size(); ++i) {
        if (_elements[i] != other._elements[i]) return false;
    }
    
    return true;
}

size_t VertFormat::GetHash() const {
    size_t hash = std::hash<uint32_t>{}(_stride);
    
    for (const auto& element : _elements) {
        // Combine hashes using XOR and bit shifting (standard hash combine pattern)
        hash ^= std::hash<int>{}(static_cast<int>(element.GetSemantic())) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>{}(static_cast<int>(element.GetType())) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(element.GetOffset()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(element.GetSemanticIndex()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    return hash;
}

// =========================================================================
// Helper Functions
// =========================================================================

uint32_t GetVertexElementTypeSize(VertexElementType type) {
    switch (type) {
        case VertexElementType::Float:              return 4;
        case VertexElementType::Float2:             return 8;
        case VertexElementType::Float3:             return 12;
        case VertexElementType::Float4:             return 16;
        case VertexElementType::Int:                return 4;
        case VertexElementType::Int2:               return 8;
        case VertexElementType::Int3:               return 12;
        case VertexElementType::Int4:               return 16;
        case VertexElementType::UByte4:             return 4;
        case VertexElementType::UByte4Normalized:   return 4;
        case VertexElementType::Short2:             return 4;
        case VertexElementType::Short2Normalized:   return 4;
        case VertexElementType::Short4:             return 8;
        case VertexElementType::Short4Normalized:   return 8;
        case VertexElementType::UShort2:            return 4;
        case VertexElementType::UShort2Normalized:  return 4;
        case VertexElementType::UShort4:            return 8;
        case VertexElementType::UShort4Normalized:  return 8;
        default:                                     return 0;
    }
}

const char* GetSemanticName(VertexElementSemantic semantic) {
    switch (semantic) {
        case VertexElementSemantic::Position:    return "POSITION";
        case VertexElementSemantic::Normal:      return "NORMAL";
        case VertexElementSemantic::Tangent:     return "TANGENT";
        case VertexElementSemantic::Bitangent:   return "BITANGENT";
        case VertexElementSemantic::Color:       return "COLOR";
        case VertexElementSemantic::TexCoord0:   return "TEXCOORD";
        case VertexElementSemantic::TexCoord1:   return "TEXCOORD";
        case VertexElementSemantic::TexCoord2:   return "TEXCOORD";
        case VertexElementSemantic::TexCoord3:   return "TEXCOORD";
        case VertexElementSemantic::BoneIndices: return "BLENDINDICES";
        case VertexElementSemantic::BoneWeights: return "BLENDWEIGHT";
        case VertexElementSemantic::Custom0:     return "CUSTOM";
        case VertexElementSemantic::Custom1:     return "CUSTOM";
        case VertexElementSemantic::Custom2:     return "CUSTOM";
        case VertexElementSemantic::Custom3:     return "CUSTOM";
        default:                                  return "UNKNOWN";
    }
}

// Helper function to convert VertexElementType to DXGI_FORMAT
DXGI_FORMAT GetDXGIFormat(VertexElementType type) {
    switch (type) {
        case VertexElementType::Float:              return DXGI_FORMAT_R32_FLOAT;
        case VertexElementType::Float2:             return DXGI_FORMAT_R32G32_FLOAT;
        case VertexElementType::Float3:             return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexElementType::Float4:             return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexElementType::Int:                return DXGI_FORMAT_R32_SINT;
        case VertexElementType::Int2:               return DXGI_FORMAT_R32G32_SINT;
        case VertexElementType::Int3:               return DXGI_FORMAT_R32G32B32_SINT;
        case VertexElementType::Int4:               return DXGI_FORMAT_R32G32B32A32_SINT;
        case VertexElementType::UByte4:             return DXGI_FORMAT_R8G8B8A8_UINT;
        case VertexElementType::UByte4Normalized:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case VertexElementType::Short2:             return DXGI_FORMAT_R16G16_SINT;
        case VertexElementType::Short2Normalized:   return DXGI_FORMAT_R16G16_SNORM;
        case VertexElementType::Short4:             return DXGI_FORMAT_R16G16B16A16_SINT;
        case VertexElementType::Short4Normalized:   return DXGI_FORMAT_R16G16B16A16_SNORM;
        case VertexElementType::UShort2:            return DXGI_FORMAT_R16G16_UINT;
        case VertexElementType::UShort2Normalized:  return DXGI_FORMAT_R16G16_UNORM;
        case VertexElementType::UShort4:            return DXGI_FORMAT_R16G16B16A16_UINT;
        case VertexElementType::UShort4Normalized:  return DXGI_FORMAT_R16G16B16A16_UNORM;
        default:                                    return DXGI_FORMAT_UNKNOWN;
    }
}

// =========================================================================
// Predefined Common Formats
// =========================================================================

namespace VertFormats {

const VertFormat& PosWUVInvW() {
    static VertFormat format = VertFormat::Builder()
        .AddFloat4(VertexElementSemantic::Position)    // XYZW (16 bytes)
        .AddFloat3(VertexElementSemantic::TexCoord0)   // UVW (12 bytes)
        .Build();
    return format;
}

const VertFormat& PosUV() {
    static VertFormat format = VertFormat::Builder()
        .AddFloat3(VertexElementSemantic::Position)    // XYZ (12 bytes)
        .AddFloat2(VertexElementSemantic::TexCoord0)   // UV (8 bytes)
        .Build();
    return format;
}

const VertFormat& PosNormUV() {
    static VertFormat format = VertFormat::Builder()
        .AddFloat3(VertexElementSemantic::Position)    // XYZ (12 bytes)
        .AddFloat3(VertexElementSemantic::Normal)      // XYZ (12 bytes)
        .AddFloat2(VertexElementSemantic::TexCoord0)   // UV (8 bytes)
        .Build();
    return format;
}

const VertFormat& PosColor() {
    static VertFormat format = VertFormat::Builder()
        .AddFloat3(VertexElementSemantic::Position)           // XYZ (12 bytes)
        .AddUByte4Normalized(VertexElementSemantic::Color)    // RGBA (4 bytes)
        .Build();
    return format;
}

} // namespace VertFormats

} // namespace ASCIIgL
