#pragma once

#include <cstddef>
#include <vector>
#include <functional>

#include <ASCIICraft/world/blockstate/BlockState.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
                    
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

        std::function<void(
            int, int, int,
            const blockstate::BlockState&,
            const uint32_t*,
            const std::array<const uint32_t*, 6>&,
            const blockstate::BlockStateRegistry&,
            std::vector<bool>&
        )> computeVisibleFaces;
    };
}