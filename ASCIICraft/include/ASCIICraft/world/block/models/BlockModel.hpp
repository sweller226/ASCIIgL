#pragma once

#include <cstddef>
#include <vector>
#include <functional>

#include <ASCIICraft/world/block/state/BlockState.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
                    
namespace blockstate {

    struct FaceRange {
        int vertByteOffset = 0;
        int vertByteCount = 0;
        int idxOffset = 0;
        int idxCount = 0;
        /// Neighbor visibility uses \ref FaceDir ordering: `0`=Top … `5`=West (same as mesh face index in
        /// cube builders). `255` skips neighbor-based culling for this quad (handled like “always visible”).
        uint8_t cardinalFace = 255;

        bool operator==(const FaceRange& o) const {
            return vertByteOffset == o.vertByteOffset &&
                   vertByteCount == o.vertByteCount &&
                   idxOffset == o.idxOffset &&
                   idxCount == o.idxCount &&
                   cardinalFace == o.cardinalFace;
        }

        bool operator!=(const FaceRange& o) const {
            return !(*this == o);
        }
    };

    struct RenderLayer {
        std::vector<std::byte> vertices;
        std::vector<int>       indices;
        std::vector<FaceRange> faces;
    };

    struct BlockModel {
        bool isFullBlock = false;
        bool opaqueNoCull = false;
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