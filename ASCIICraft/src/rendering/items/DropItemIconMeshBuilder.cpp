#include <ASCIICraft/rendering/items/DropItemIconMeshBuilder.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <vector>

namespace rendering::items {
namespace {

constexpr float kModelUnits = 16.0f;
constexpr float kMinZ = 7.5f;
constexpr float kMaxZ = 8.5f;
constexpr float kUvShrink = 0.001f;
constexpr float kToBlock = 1.0f / kModelUnits;
constexpr int kAlphaThreshold = 1;

enum class SideDirection { Up, Down, Left, Right };

struct SideFace {
    SideDirection facing = SideDirection::Up;
    int min = 0;
    int max = 0;
    int anchor = 0;
};

bool IsHorizontal(SideDirection facing) {
    return facing == SideDirection::Up || facing == SideDirection::Down;
}

bool IsPixelTransparent(const ASCIIgL::Texture& texture, int x, int y) {
    if (x < 0 || y < 0 || x >= texture.GetWidth() || y >= texture.GetHeight()) {
        return true;
    }
    const uint8_t* rgba = texture.GetPixelRGBAPtr(x, y);
    return rgba[3] < kAlphaThreshold;
}

int NeighborX(SideDirection facing) {
    switch (facing) {
        case SideDirection::Left: return -1;
        case SideDirection::Right: return 1;
        default: return 0;
    }
}

int NeighborY(SideDirection facing) {
    switch (facing) {
        case SideDirection::Up: return -1;
        case SideDirection::Down: return 1;
        default: return 0;
    }
}

struct SideFaceLess {
    bool operator()(const SideFace& a, const SideFace& b) const {
        if (a.facing != b.facing) return a.facing < b.facing;
        if (a.anchor != b.anchor) return a.anchor < b.anchor;
        if (a.min != b.min) return a.min < b.min;
        return a.max < b.max;
    }
};

void InsertOrMergeFace(std::set<SideFace, SideFaceLess>& sideFaces, SideFace face) {
    while (true) {
        const int newAnchor = face.anchor;
        const int newMin = face.min;
        const int newMax = face.max;
        bool merged = false;

        for (auto it = sideFaces.begin(); it != sideFaces.end(); ++it) {
            const SideFace& oldFace = *it;
            if (oldFace.facing != face.facing || oldFace.anchor != newAnchor) {
                continue;
            }

            const int oldMin = oldFace.min;
            const int oldMax = oldFace.max;

            if (newMin == oldMax + 1) {
                merged = true;
                face = SideFace{face.facing, oldMin, newMax, newAnchor};
            } else if (newMax == oldMin - 1) {
                merged = true;
                face = SideFace{face.facing, newMin, oldMax, newAnchor};
            }

            if (merged) {
                sideFaces.erase(it);
                break;
            }
        }

        if (!merged) {
            sideFaces.insert(face);
            break;
        }
    }
}

void TryInsertFace(
    SideDirection facing,
    std::set<SideFace, SideFaceLess>& sideFaces,
    const ASCIIgL::Texture& texture,
    int pixelX,
    int pixelY
) {
    const int neighborX = pixelX + NeighborX(facing);
    const int neighborY = pixelY + NeighborY(facing);
    if (!IsPixelTransparent(texture, neighborX, neighborY)) {
        return;
    }

    const SideFace seed = IsHorizontal(facing)
        ? SideFace{facing, pixelX, pixelX, pixelY}
        : SideFace{facing, pixelY, pixelY, pixelX};
    InsertOrMergeFace(sideFaces, seed);
}

std::set<SideFace, SideFaceLess> BuildSideFaces(const ASCIIgL::Texture& texture) {
    const int width = texture.GetWidth();
    const int height = texture.GetHeight();
    std::set<SideFace, SideFaceLess> sideFaces;

    for (int pixelY = 0; pixelY < height; ++pixelY) {
        for (int pixelX = 0; pixelX < width; ++pixelX) {
            if (IsPixelTransparent(texture, pixelX, pixelY)) {
                continue;
            }

            TryInsertFace(SideDirection::Up, sideFaces, texture, pixelX, pixelY);
            TryInsertFace(SideDirection::Down, sideFaces, texture, pixelX, pixelY);
            TryInsertFace(SideDirection::Left, sideFaces, texture, pixelX, pixelY);
            TryInsertFace(SideDirection::Right, sideFaces, texture, pixelX, pixelY);
        }
    }

    return sideFaces;
}

glm::vec3 ModelToWorld(float x, float y, float z) {
    return glm::vec3(
        x * kToBlock - 0.5f,
        y * kToBlock - 0.5f,
        z * kToBlock - 0.5f
    );
}

using V = ASCIIgL::VertStructs::PosUV;

void AppendQuad(
    std::vector<V>& vertices,
    std::vector<int>& indices,
    const std::array<glm::vec3, 4>& positions,
    const std::array<glm::vec2, 4>& uvs
) {
    const int base = static_cast<int>(vertices.size());
    for (int i = 0; i < 4; ++i) {
        V vertex;
        vertex.SetXYZ(positions[i]);
        vertex.SetUV(uvs[i]);
        vertices.push_back(vertex);
    }
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

void AppendPlateFaces(
    std::vector<V>& vertices,
    std::vector<int>& indices
) {
    const float z0 = kMinZ;
    const float z1 = kMaxZ;

    const glm::vec3 p000 = ModelToWorld(0.0f, 0.0f, z0);
    const glm::vec3 p100 = ModelToWorld(kModelUnits, 0.0f, z0);
    const glm::vec3 p110 = ModelToWorld(kModelUnits, kModelUnits, z0);
    const glm::vec3 p010 = ModelToWorld(0.0f, kModelUnits, z0);

    const glm::vec3 p001 = ModelToWorld(0.0f, 0.0f, z1);
    const glm::vec3 p101 = ModelToWorld(kModelUnits, 0.0f, z1);
    const glm::vec3 p111 = ModelToWorld(kModelUnits, kModelUnits, z1);
    const glm::vec3 p011 = ModelToWorld(0.0f, kModelUnits, z1);

    // South (+Z): full layer0 texture.
    AppendQuad(
        vertices,
        indices,
        {p001, p101, p111, p011},
        {glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f)}
    );

    // North (-Z): mirrored for outward-facing winding.
    AppendQuad(
        vertices,
        indices,
        {p100, p000, p010, p110},
        {glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f)}
    );
}

void AppendSideFace(
    std::vector<V>& vertices,
    std::vector<int>& indices,
    const SideFace& face,
    int spriteWidth,
    int spriteHeight
) {
    const float xScale = kModelUnits / static_cast<float>(spriteWidth);
    const float yScale = kModelUnits / static_cast<float>(spriteHeight);
    const bool horizontal = IsHorizontal(face.facing);

    const float minCoord = static_cast<float>(face.min);
    const float maxCoord = static_cast<float>(face.max);
    const float anchor = static_cast<float>(face.anchor);
    const float length = maxCoord - minCoord + 1.0f;

    float minX = horizontal ? minCoord : anchor;
    float minY = horizontal ? anchor : minCoord;

    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;

    if (horizontal) {
        u0 = minX + kUvShrink;
        v0 = minY + kUvShrink;
        u1 = minX + length - kUvShrink;
        v1 = minY + 1.0f - kUvShrink;
    } else {
        u0 = minX + kUvShrink;
        v0 = minY + length - kUvShrink;
        u1 = minX + 1.0f - kUvShrink;
        v1 = minY + kUvShrink;
    }

    float fromX = minX;
    float fromY = minY;
    float toX = minX;
    float toY = minY;

    switch (face.facing) {
        case SideDirection::Up:
            toX = minX + length;
            break;
        case SideDirection::Left:
            toY = minY + length;
            break;
        case SideDirection::Down:
            fromY = minY + 1.0f;
            toY = minY + 1.0f;
            toX = minX + length;
            break;
        case SideDirection::Right:
            fromX = minX + 1.0f;
            toX = minX + 1.0f;
            toY = minY + length;
            break;
    }

    fromX *= xScale;
    fromY *= yScale;
    toX *= xScale;
    toY *= yScale;

    fromY = kModelUnits - fromY;
    toY = kModelUnits - toY;

    switch (face.facing) {
        case SideDirection::Right:
            fromX = toX;
            break;
        case SideDirection::Down:
            fromY = toY;
            break;
        case SideDirection::Left:
            toX = fromX;
            break;
        case SideDirection::Up:
            toY = fromY;
            break;
    }

    const float tu0 = u0 * xScale;
    const float tv0 = v0 * yScale;
    const float tu1 = u1 * xScale;
    const float tv1 = v1 * yScale;

    const float bx0 = std::min(fromX, toX);
    const float bx1 = std::max(fromX, toX);
    const float by0 = std::min(fromY, toY);
    const float by1 = std::max(fromY, toY);
    const float bz0 = kMinZ;
    const float bz1 = kMaxZ;

    const glm::vec3 p000 = ModelToWorld(bx0, by0, bz0);
    const glm::vec3 p100 = ModelToWorld(bx1, by0, bz0);
    const glm::vec3 p110 = ModelToWorld(bx1, by1, bz0);
    const glm::vec3 p010 = ModelToWorld(bx0, by1, bz0);
    const glm::vec3 p001 = ModelToWorld(bx0, by0, bz1);
    const glm::vec3 p101 = ModelToWorld(bx1, by0, bz1);
    const glm::vec3 p111 = ModelToWorld(bx1, by1, bz1);
    const glm::vec3 p011 = ModelToWorld(bx0, by1, bz1);

    const glm::vec2 uv00(tu0, tv1);
    const glm::vec2 uv10(tu1, tv1);
    const glm::vec2 uv11(tu1, tv0);
    const glm::vec2 uv01(tu0, tv0);

    switch (face.facing) {
        case SideDirection::Up:
            AppendQuad(vertices, indices, {p010, p110, p111, p011}, {uv00, uv10, uv11, uv01});
            break;
        case SideDirection::Down:
            AppendQuad(vertices, indices, {p100, p000, p001, p101}, {uv00, uv10, uv11, uv01});
            break;
        case SideDirection::Left:
            AppendQuad(vertices, indices, {p000, p010, p011, p001}, {uv00, uv10, uv11, uv01});
            break;
        case SideDirection::Right:
            AppendQuad(vertices, indices, {p110, p100, p101, p111}, {uv00, uv10, uv11, uv01});
            break;
    }
}

} // namespace

std::shared_ptr<ASCIIgL::Mesh> DropItemIconMeshBuilder::Build(
    const std::shared_ptr<ASCIIgL::Texture>& texture
) {
    if (!texture || texture->GetWidth() <= 0 || texture->GetHeight() <= 0) {
        return nullptr;
    }

    std::vector<V> vertices;
    std::vector<int> indices;
    vertices.reserve(256);
    indices.reserve(384);

    AppendPlateFaces(vertices, indices);

    const std::set<SideFace, SideFaceLess> sideFaces = BuildSideFaces(*texture);
    for (const SideFace& face : sideFaces) {
        AppendSideFace(vertices, indices, face, texture->GetWidth(), texture->GetHeight());
    }

    if (vertices.empty() || indices.empty()) {
        return nullptr;
    }

    std::vector<std::byte> byteVertices(
        reinterpret_cast<std::byte*>(vertices.data()),
        reinterpret_cast<std::byte*>(vertices.data()) + vertices.size() * sizeof(V)
    );

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(byteVertices),
        ASCIIgL::VertFormats::PosUV(),
        std::move(indices),
        texture.get()
    );
}

} // namespace rendering::items
