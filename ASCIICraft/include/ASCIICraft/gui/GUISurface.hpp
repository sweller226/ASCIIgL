#pragma once

#include <glm/vec2.hpp>
#include <memory>

namespace ASCIIgL {
class Mesh;
class Material;
}

namespace gui {
    struct GUISurface {
        std::shared_ptr<ASCIIgL::Mesh>     mesh;
        std::shared_ptr<ASCIIgL::Material> material;
    };
}

