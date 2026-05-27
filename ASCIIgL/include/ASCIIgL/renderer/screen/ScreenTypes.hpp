#pragma once

#include <cstdint>

namespace ASCIIgL {

using NativeWindowHandle = void*;

struct ScreenPixel {
    wchar_t glyph = L' ';
    uint16_t attributes = 0;
};

} // namespace ASCIIgL
