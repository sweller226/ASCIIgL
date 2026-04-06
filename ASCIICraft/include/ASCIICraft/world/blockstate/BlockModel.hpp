#pragma once

#include <cstddef>
#include <vector>

namespace blockstate {

    struct FaceRange {
        int vertByteOffset;
        int vertByteCount;
        int idxOffset;
        int idxCount;
    };

    struct RenderLayer {
        std::vector<std::byte> vertices;
        std::vector<int>       indices;
        std::vector<FaceRange> faces;
    };

    struct BlockModel {
        bool isFullBlock = false;
        RenderLayer opaque;
        RenderLayer transparent;
    };

}