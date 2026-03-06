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

}