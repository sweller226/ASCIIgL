#pragma once

#include <glm/vec2.hpp>

#include <memory>

namespace ASCIIgL { class Mesh; }

namespace gui::text {

struct TextLabelMesh {
    std::shared_ptr<ASCIIgL::Mesh> mesh;
    glm::vec2 contentSizePx{0.0f};

    explicit operator bool() const { return mesh != nullptr; }
};

} // namespace gui::text
