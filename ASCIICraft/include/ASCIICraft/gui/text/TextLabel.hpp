#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <string>

namespace gui::text {

enum class TextWrapMode {
    None,
    Word,
    Character,
};

enum class TextOverflowMode {
    Clip,
    Ellipsis,
};

enum class TextHAlign {
    Left,
    Center,
    Right,
};

enum class TextVAlign {
    Top,
    Middle,
    Bottom,
};

struct TextLabel {
    std::string meshKey;
    glm::vec2 position{0.0f, 0.0f};

    float scale = 1.0f;
    float lineHeightMultiplier = 1.0f;
    float letterSpacing = 0.0f;
    float wordSpacing = 0.0f;

    TextWrapMode wrapMode = TextWrapMode::None;
    TextOverflowMode overflowMode = TextOverflowMode::Clip;
    TextHAlign horizontalAlign = TextHAlign::Left;
    TextVAlign verticalAlign = TextVAlign::Top;

    // Wrap width in pixels (<= 0 disables wrapping).
    float maxWidthPx = 0.0f;
    // Optional label bounds for alignment/overflow logic.
    glm::vec2 boundsPx{0.0f, 0.0f};

    // Color/tint placeholders for future shader support.
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 shadowColor{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec2 shadowOffsetPx{0.0f, 0.0f};
};

} // namespace gui::text
