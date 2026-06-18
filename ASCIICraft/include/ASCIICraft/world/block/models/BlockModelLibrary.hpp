#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <ASCIICraft/world/block/models/BlockModel.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace blockmodels {
    struct CubeSpec;

    // Mesh data already in the engine's expected vertex representation:
    // - vertices are raw bytes for PosUVLayer (terrain texture array)
    // - indices reference the local vertex array

    // Cache of block-local models keyed by flattened stateId.
    // Chunk mesh generation can translate/append this geometry into chunk buffers.
    class BlockModelLibrary {
    public:
        using ModelPtr = std::shared_ptr<const blockstate::BlockModel>;

        // Returns a registered model if present; otherwise returns nullptr.
        // Models are expected to be registered explicitly via RegisterModel().
        const blockstate::BlockModel* GetModel(uint32_t stateId) const;
        // Deterministic equal-probability selection by world-space block coordinate.
        const blockstate::BlockModel* GetModelForBlock(uint32_t stateId, int worldX, int worldY, int worldZ) const;

        // Explicitly register a model for a specific stateId.
        void RegisterModel(uint32_t stateId, ModelPtr model, blockstate::BlockStateRegistry& bsr);
        // Explicitly register one shared model for every state in a type.
        void RegisterModel(uint16_t typeId, ModelPtr model, blockstate::BlockStateRegistry& bsr);
        // Explicitly register a model-set (equal-probability entries) for one stateId.
        void RegisterModelSet(uint32_t stateId, const std::vector<ModelPtr>& models, blockstate::BlockStateRegistry& bsr);
        // Explicitly register one model-set for every state in a type.
        void RegisterModelSet(uint16_t typeId, const std::vector<ModelPtr>& models, blockstate::BlockStateRegistry& bsr);
        // Convenience: build a cube model from spec, then register it for all states in a type.
        void RegisterCubeModel(uint16_t typeId, const CubeSpec& spec, blockstate::BlockStateRegistry& bsr);

    private:
        // stateId -> model set (fast path)
        std::vector<std::vector<ModelPtr>> stateModelSets_;
        // fingerprint -> bucket of canonical models that share the same fingerprint.
        // We still compare for equality to avoid sharing on hash collisions.
        std::unordered_map<size_t, std::vector<ModelPtr>> modelsByFingerprint_;
    };
} // namespace blockmodels
