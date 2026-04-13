// ecs/components/Renderable.hpp
#pragma once

#include <memory>
#include <cstdint>
#include <string>

#include <glm/vec3.hpp>

namespace ASCIIgL { class Mesh; class Texture; }

namespace ecs::components {

enum class RenderType : uint8_t {
    NOT_SET,
    ELEM_3D,
    ELEM_2D,
};
    
struct Renderable {
    // Shared ownership of mesh data. Mesh contains its own GPU cache pointer,
    // so the component only needs to reference the mesh object.

    RenderType renderType{RenderType::NOT_SET};
    std::shared_ptr<ASCIIgL::Mesh> mesh{nullptr};
    int32_t layer{0};
    bool visible{true};

    // Material name to look up in MaterialLibrary. Empty = use default material.
    std::string materialName;

    // When true, both sides of faces are rendered for this entity.
    // Used for mobs with see-through geometry (skeleton ribs/spine visible
    // from front) or geometry that faces away from the camera (chicken feet).
    // Propagated through DrawItem -> DrawCall -> ExecuteDrawList rasterizer toggle.
    bool disableBackfaceCulling{false};

    void SetMesh(std::shared_ptr<ASCIIgL::Mesh> m) {
        mesh = std::move(m);
    }
};

// Held item rendering — separate mesh+texture rendered at a local-space offset
// from the entity origin (e.g. skeleton's bow).  Both faces rendered so the
// item is visible regardless of camera angle.
//
// positionOffset is in entity-local space (mesh space), so it rotates with the
// entity — the item stays anchored to the hand as the mob turns.
struct HeldItemRenderable {
    std::shared_ptr<ASCIIgL::Mesh> mesh;
    std::string  materialName;
    glm::vec3    positionOffset{0.f, 0.f, 0.f}; // entity-local, in world units
    // rotation: Euler angles (radians) applied as Rx * Ry * Rz (X first, then Y, then Z).
    // scale uniformly shrinks/grows the item mesh before rotation.
    // Matrix applied as: T(positionOffset) * Rx * Ry * Rz * S(scale)
    glm::vec3    rotation{0.f, 0.f, 0.f};  // radians (X, Y, Z)
    float        scale                    = 1.f;
    bool         visible                  = true;
    int32_t      layer                    = 1;
    bool         disableBackfaceCulling   = true;  // sprite visible from both sides
};

// Fur/wool layer rendering — overlay identical mesh with different texture.
// Used for sheep (wool layer on top of skin) rendered with depth offset.
// Renders in a second pass after main renderable to appear on top.
struct FurRenderable {
    std::shared_ptr<ASCIIgL::Mesh> mesh;
    std::string  materialName;
    glm::vec3    positionOffset{0.f, 0.f, 0.f};
    bool         visible                  = true;
    int32_t      layer                    = 1;
    bool         disableBackfaceCulling   = false;
};

}