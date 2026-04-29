#pragma once

#include <ASCIICraft/gui/text/BitmapFont.hpp>
#include <ASCIICraft/gui/text/TextLabel.hpp>

#include <glm/vec2.hpp>

#include <memory>
#include <string_view>

namespace ASCIIgL { class Mesh; }

namespace gui::text {

class TextLabelMeshBuilder {
public:
    static std::shared_ptr<ASCIIgL::Mesh> BuildMesh(const BitmapFont& font,
                                                    std::string_view text,
                                                    float scale,
                                                    const TextLabel& label,
                                                    glm::vec2 labelSizePx);
};

} // namespace gui::text
