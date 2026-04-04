#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <ASCIICraft/world/blockstate/BlockModel.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

namespace blockstate {
    struct BlockState;
    struct CubeSpec;
    class BlockStateRegistry;

    // Mesh data already in the engine's expected vertex representation:
    // - vertices are raw bytes for a specific vertex format (PosUVLayerLight in your current pipeline)
    // - indices reference the local vertex array

    // Cache of block-local models keyed by flattened stateId.
    // Chunk mesh generation can translate/append this geometry into chunk buffers.
    class BlockModelLibrary {
    public:
        using ModelPtr = std::shared_ptr<const BlockModel>;

        // Returns a registered model if present; otherwise returns nullptr.
        // Models are expected to be registered explicitly via RegisterModel().
        ModelPtr GetModel(uint32_t stateId) const;

        // Explicitly register a model for a specific stateId.
        void RegisterModel(uint32_t stateId, std::shared_ptr<const BlockModel> model, BlockStateRegistry& bsr);
        // Explicitly register one shared model for every state in a type.
        void RegisterModel(uint16_t typeId, std::shared_ptr<const BlockModel> model, BlockStateRegistry& bsr);
        // Convenience: build a cube model from spec, then register it for all states in a type.
        void RegisterCubeModel(uint16_t typeId, const CubeSpec& spec, BlockStateRegistry& bsr);

    private:
        // stateId -> shared model pointer (fast path)
        std::vector<ModelPtr> modelsByState_;
        // fingerprint -> bucket of canonical models that share the same fingerprint.
        // We still compare for equality to avoid sharing on hash collisions.
        std::unordered_map<size_t, std::vector<ModelPtr>> modelsByFingerprint_;
    };
} // namespace blockstate
