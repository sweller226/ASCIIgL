#pragma once

#include <ASCIICraft/world/blockstate/FaceDir.hpp>
#include <ASCIICraft/world/blockstate/BlockModel.hpp>

// Helper functions for building cube-like block models.
// Keep this layer independent of BlockState so multiple states can share the same model.

#include <array>

#include <glm/vec3.hpp>

namespace blockstate {

static constexpr int FACE_DIR_COUNT = 6;

struct CubeSpec {
    // Texture layer index per face in FaceDir order.
    std::array<int, FACE_DIR_COUNT> faceLayers{ {0, 0, 0, 0, 0, 0} };

    FaceDir facing = FaceDir::North;

    // If true, all faces go to transparent buffers; otherwise opaque buffers.
    bool transparent = false;

    // ---------------------------------------------------------------------
    // Fluent helpers (mutate this spec)
    // ---------------------------------------------------------------------
    CubeSpec& SetAllFaces(int layer) {
        faceLayers = { layer, layer, layer, layer, layer, layer };
        return *this;
    }

    CubeSpec& SetFace(FaceDir face, int layer) {
        faceLayers[static_cast<int>(face)] = layer;
        return *this;
    }

    CubeSpec& SetFacing(FaceDir f) {
        facing = f;
        return *this;
    }

    CubeSpec& SetTransparent(bool v = true) {
        transparent = v;
        return *this;
    }
};

BlockModel BuildCubeModel(const CubeSpec& spec);

// -------------------------------------------------------------------------
// Default-friendly factories (return a configured spec)
// -------------------------------------------------------------------------
inline CubeSpec CubeFull() {
    return CubeSpec{};
}

inline CubeSpec CubeFullAllFaces(int layer, bool transparent = false, FaceDir facing = FaceDir::North) {
    CubeSpec s;
    s.SetAllFaces(layer);
    s.facing = facing;
    s.transparent = transparent;
    return s;
}

inline CubeSpec CubeFullTopBottomSides(int top, int bottom, int side, bool transparent = false, FaceDir facing = FaceDir::North) {
    CubeSpec s;
    s.SetAllFaces(side);
    s.SetFace(FaceDir::Top, top);
    s.SetFace(FaceDir::Bottom, bottom);
    s.facing = facing;
    s.transparent = transparent;
    return s;
}

} // namespace blockstate