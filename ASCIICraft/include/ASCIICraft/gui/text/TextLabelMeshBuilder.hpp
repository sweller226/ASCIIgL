#pragma once

#include <ASCIICraft/gui/text/BitmapFont.hpp>
#include <ASCIICraft/gui/text/TextLabelMesh.hpp>
#include <ASCIICraft/gui/text/TextLabelStyle.hpp>

#include <glm/vec2.hpp>

#include <string_view>

namespace gui::text {

class TextLabelMeshBuilder {
public:
    static TextLabelMesh Build(const BitmapFont& font,
                               std::string_view text,
                               float scale,
                               const TextLabelStyle& style);
};

} // namespace gui::text
