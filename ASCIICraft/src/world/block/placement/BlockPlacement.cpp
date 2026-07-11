#include <ASCIICraft/world/block/placement/BlockPlacement.hpp>
#include <ASCIICraft/world/block/placement/FencePlacement.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>

#include <algorithm>

namespace {

uint32_t ApplyPlayerFacing(
    const blockstate::BlockStateRegistry& bsr,
    uint32_t stateId,
    std::optional<FaceDir> faceDir
) {
    if (!faceDir || !bsr.IsValidState(stateId)) {
        return stateId;
    }

    const uint16_t typeId = bsr.GetTypeIdFromState(stateId);
    const auto& type = bsr.GetType(typeId);
    const char* facingValue = FaceDirToString(*faceDir);

    const auto facingProperty = std::find_if(
        type.properties.begin(),
        type.properties.end(),
        [](const blockstate::BlockProperty& property) {
            return property.name == "facing";
        }
    );
    if (facingProperty == type.properties.end()) {
        return stateId;
    }

    const auto& allowedValues = facingProperty->allowedValues;
    if (std::find(allowedValues.begin(), allowedValues.end(), facingValue) == allowedValues.end()) {
        return stateId;
    }

    return bsr.WithProperty(stateId, "facing", facingValue);
}

} // namespace

namespace blockplacement {

    uint32_t FinalizePlacedState(
        const blockstate::BlockStateRegistry& bsr,
        const ChunkManager& chunkManager,
        uint32_t stateId,
        const WorldCoord& position,
        PlacementContext context,
        const bool keepStateId,
        std::optional<FaceDir> faceDir
    ) {
        if (keepStateId) {
            return stateId;
        }
    
        if (!bsr.IsValidState(stateId)) {
            return stateId;
        }

        stateId = ApplyPlayerFacing(bsr, stateId, faceDir);

        const uint16_t placedTypeId = bsr.GetTypeIdFromState(stateId);
        const auto& placedType = bsr.GetType(placedTypeId);
        if (!detail::IsFenceTypeName(placedType.name)) {
            return stateId;
        }

        return detail::FinalizeFencePlacedState(bsr, chunkManager, stateId, position);
    }

    uint32_t FinalizePlacedState(
        const blockstate::BlockStateRegistry& bsr,
        const ChunkManager& chunkManager,
        uint32_t stateId,
        int x, int y, int z,
        PlacementContext context,
        const bool keepStateId,
        std::optional<FaceDir> faceDir
    ) {
        return FinalizePlacedState(bsr, chunkManager, stateId, WorldCoord(x, y, z), context, keepStateId, faceDir);
    }

    uint32_t FinalizePlacedStateBasic(
        const blockstate::BlockStateRegistry& bsr,
        uint32_t stateId,
        const WorldCoord& position,
        PlacementContext context,
        const bool keepStateId,
        std::optional<FaceDir> faceDir
    ) {
        (void)position;
        (void)context;
        if (keepStateId) {
            return stateId;
        }
        return ApplyPlayerFacing(bsr, stateId, faceDir);
    }

    uint32_t FinalizePlacedStateBasic(
        const blockstate::BlockStateRegistry& bsr,
        uint32_t stateId,
        int x, int y, int z,
        PlacementContext context,
        const bool keepStateId,
        std::optional<FaceDir> faceDir
    ) {
        return FinalizePlacedStateBasic(bsr, stateId, WorldCoord(x, y, z), context, keepStateId, faceDir);
    }
}
