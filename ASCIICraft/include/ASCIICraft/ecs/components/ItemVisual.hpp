#pragma once

#include <memory>

namespace ASCIIgL { class Mesh; }

namespace ecs::components {

/// Visual data for an item definition entity.
/// Kept separate from Renderable so prototype entities are not
/// processed by the render system.
struct ItemVisual {
    std::shared_ptr<ASCIIgL::Mesh> mesh;
    bool is2DIcon = false;  // If true, use icon texture array instead of block texture array
};

} // namespace ecs::components
