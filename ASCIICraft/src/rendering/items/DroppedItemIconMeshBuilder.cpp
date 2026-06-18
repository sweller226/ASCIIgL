#include <ASCIICraft/rendering/items/DroppedItemIconMeshBuilder.hpp>

#include <ASCIICraft/rendering/items/ItemModelSpace.hpp>
#include <ASCIICraft/util/MeshBuilderUtil.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
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

constexpr float kUvShrink = 0.001f;
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

bool IsPixelTransparent(const uint8_t* rgba, int width, int height, int x, int y) {
    if (!rgba || x < 0 || y < 0 || x >= width || y >= height) {
        return true;
    }
    const int index = (y * width + x) * 4;
    return rgba[index + 3] < kAlphaThreshold;
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
    const uint8_t* rgba,
    int width,
    int height,
    int pixelX,
    int pixelY
) {
    const int neighborX = pixelX + NeighborX(facing);
    const int neighborY = pixelY + NeighborY(facing);
    if (!IsPixelTransparent(rgba, width, height, neighborX, neighborY)) {
        return;
    }

    const SideFace seed = IsHorizontal(facing)
        ? SideFace{facing, pixelX, pixelX, pixelY}
        : SideFace{facing, pixelY, pixelY, pixelX};
    InsertOrMergeFace(sideFaces, seed);
}

std::set<SideFace, SideFaceLess> BuildSideFaces(const uint8_t* rgba, int width, int height) {
    std::set<SideFace, SideFaceLess> sideFaces;

    for (int pixelY = 0; pixelY < height; ++pixelY) {
        for (int pixelX = 0; pixelX < width; ++pixelX) {
            if (IsPixelTransparent(rgba, width, height, pixelX, pixelY)) {
                continue;
            }

            TryInsertFace(SideDirection::Up, sideFaces, rgba, width, height, pixelX, pixelY);
            TryInsertFace(SideDirection::Down, sideFaces, rgba, width, height, pixelX, pixelY);
            TryInsertFace(SideDirection::Left, sideFaces, rgba, width, height, pixelX, pixelY);
            TryInsertFace(SideDirection::Right, sideFaces, rgba, width, height, pixelX, pixelY);
        }
    }

    return sideFaces;
}

using V = ASCIIgL::VertStructs::PosUVLayer;

void AppendPlateFaces(
    std::vector<V>& vertices,
    std::vector<int>& indices,
    float layer
) {
    const float z0 = kItemPlateMinZ;
    const float z1 = kItemPlateMaxZ;

    const glm::vec3 p000 = ModelUnitsToBlockCentered(0.0f, 0.0f, z0);
    const glm::vec3 p100 = ModelUnitsToBlockCentered(kItemModelUnitsPerBlock, 0.0f, z0);
    const glm::vec3 p110 = ModelUnitsToBlockCentered(kItemModelUnitsPerBlock, kItemModelUnitsPerBlock, z0);
    const glm::vec3 p010 = ModelUnitsToBlockCentered(0.0f, kItemModelUnitsPerBlock, z0);

    const glm::vec3 p001 = ModelUnitsToBlockCentered(0.0f, 0.0f, z1);
    const glm::vec3 p101 = ModelUnitsToBlockCentered(kItemModelUnitsPerBlock, 0.0f, z1);
    const glm::vec3 p111 = ModelUnitsToBlockCentered(kItemModelUnitsPerBlock, kItemModelUnitsPerBlock, z1);
    const glm::vec3 p011 = ModelUnitsToBlockCentered(0.0f, kItemModelUnitsPerBlock, z1);

    // South (+Z): full layer0 texture.
    util::AppendQuadPosUVLayer(
        vertices,
        indices,
        {p001, p101, p111, p011},
        {glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
        layer
    );

    // North (-Z): match south UV-to-corner mapping when viewed from outside.
    util::AppendQuadPosUVLayer(
        vertices,
        indices,
        {p000, p100, p110, p010},
        {glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
        layer
    );
}

void AppendSideFace(
    std::vector<V>& vertices,
    std::vector<int>& indices,
    const SideFace& face,
    int spriteWidth,
    int spriteHeight,
    float layer
) {
    const float xScale = kItemModelUnitsPerBlock / static_cast<float>(spriteWidth);
    const float yScale = kItemModelUnitsPerBlock / static_cast<float>(spriteHeight);
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

    fromY = kItemModelUnitsPerBlock - fromY;
    toY = kItemModelUnitsPerBlock - toY;

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

    const float invSpriteWidth = 1.0f / static_cast<float>(spriteWidth);
    const float invSpriteHeight = 1.0f / static_cast<float>(spriteHeight);
    const float tu0 = u0 * invSpriteWidth;
    const float tv0 = v0 * invSpriteHeight;
    const float tu1 = u1 * invSpriteWidth;
    const float tv1 = v1 * invSpriteHeight;

    const float bx0 = std::min(fromX, toX);
    const float bx1 = std::max(fromX, toX);
    const float by0 = std::min(fromY, toY);
    const float by1 = std::max(fromY, toY);
    const float bz0 = kItemPlateMinZ;
    const float bz1 = kItemPlateMaxZ;

    const glm::vec3 p000 = ModelUnitsToBlockCentered(bx0, by0, bz0);
    const glm::vec3 p100 = ModelUnitsToBlockCentered(bx1, by0, bz0);
    const glm::vec3 p110 = ModelUnitsToBlockCentered(bx1, by1, bz0);
    const glm::vec3 p010 = ModelUnitsToBlockCentered(bx0, by1, bz0);
    const glm::vec3 p001 = ModelUnitsToBlockCentered(bx0, by0, bz1);
    const glm::vec3 p101 = ModelUnitsToBlockCentered(bx1, by0, bz1);
    const glm::vec3 p111 = ModelUnitsToBlockCentered(bx1, by1, bz1);
    const glm::vec3 p011 = ModelUnitsToBlockCentered(bx0, by1, bz1);

    const glm::vec2 uv00(tu0, tv1);
    const glm::vec2 uv10(tu1, tv1);
    const glm::vec2 uv11(tu1, tv0);
    const glm::vec2 uv01(tu0, tv0);

    switch (face.facing) {
        case SideDirection::Up:
            util::AppendQuadPosUVLayer(vertices, indices, {p010, p110, p111, p011}, {uv00, uv10, uv11, uv01}, layer);
            break;
        case SideDirection::Down:
            util::AppendQuadPosUVLayer(vertices, indices, {p100, p000, p001, p101}, {uv00, uv10, uv11, uv01}, layer);
            break;
        case SideDirection::Left:
            util::AppendQuadPosUVLayer(vertices, indices, {p000, p010, p011, p001}, {uv00, uv10, uv11, uv01}, layer);
            break;
        case SideDirection::Right:
            util::AppendQuadPosUVLayer(vertices, indices, {p110, p100, p101, p111}, {uv00, uv10, uv11, uv01}, layer);
            break;
    }
}

} // namespace

std::shared_ptr<ASCIIgL::Mesh> DroppedItemIconMeshBuilder::Build(
    float iconLayer,
    const std::shared_ptr<ASCIIgL::TextureArray>& textureArray
) {
    if (!textureArray || !textureArray->IsValid()) {
        return nullptr;
    }

    const int layer = static_cast<int>(iconLayer);
    if (layer < 0 || layer >= textureArray->GetLayerCount()) {
        return nullptr;
    }

    const int width = textureArray->GetMipWidth(0);
    const int height = textureArray->GetMipHeight(0);
    const uint8_t* rgba = textureArray->GetLayerData(layer, 0);
    if (!rgba || width <= 0 || height <= 0) {
        return nullptr;
    }

    std::vector<V> vertices;
    std::vector<int> indices;
    vertices.reserve(256);
    indices.reserve(384);

    AppendPlateFaces(vertices, indices, iconLayer);

    const std::set<SideFace, SideFaceLess> sideFaces = BuildSideFaces(rgba, width, height);
    for (const SideFace& face : sideFaces) {
        AppendSideFace(vertices, indices, face, width, height, iconLayer);
    }

    if (vertices.empty() || indices.empty()) {
        return nullptr;
    }

    return std::make_shared<ASCIIgL::Mesh>(
        util::PackVerts(vertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(indices),
        textureArray.get()
    );
}

} // namespace rendering::items
