#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace blockstate {
    struct BlockState;
    class BlockStateRegistry;

    // Mesh data already in the engine's expected vertex representation:
    // - vertices are raw bytes for a specific vertex format (PosUVLayerLight in your current pipeline)
    // - indices reference the local vertex array
    struct BlockModel {
        std::vector<std::byte> opaqueVertices;
        std::vector<int> opaqueIndices;
        std::vector<std::byte> transparentVertices;
        std::vector<int> transparentIndices;
    };

    // Cache of block-local models keyed by flattened stateId.
    // Chunk mesh generation can translate/append this geometry into chunk buffers.
    class BlockModelLibrary {
    public:
        using ModelPtr = std::shared_ptr<const BlockModel>;

        // Returns a registered model if present; otherwise returns nullptr.
        // Models are expected to be registered explicitly via RegisterModel().
        ModelPtr GetModel(uint32_t stateId, const BlockStateRegistry& bsr);

        // Explicitly register a model for a specific stateId.
        void RegisterModel(uint32_t stateId, std::shared_ptr<const BlockModel> model);

    private:
        // stateId -> shared model pointer (fast path)
        std::unordered_map<uint32_t, ModelPtr> modelsByState_;
        // fingerprint -> bucket of canonical models that share the same fingerprint.
        // We still compare for equality to avoid sharing on hash collisions.
        std::unordered_map<size_t, std::vector<ModelPtr>> modelsByFingerprint_;
    };
} // namespace blockstate
