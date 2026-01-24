// ecs/components/Renderable.hpp
#pragma once

#include <memory>
#include <cstdint>

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

    void SetMesh(std::shared_ptr<ASCIIgL::Mesh> m) {
        mesh = std::move(m);
    }
};

}