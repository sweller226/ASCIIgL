#include <ASCIICraft/world/block/placement/BlockPlacement.hpp>
#include <ASCIICraft/world/block/placement/FencePlacement.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>

#include <algorithm>
#include <string>

namespace {

FaceDir OppositeHorizontalFaceDir(FaceDir face) {
    switch (face) {
        case FaceDir::North: return FaceDir::South;
        case FaceDir::South: return FaceDir::North;
        case FaceDir::East:  return FaceDir::West;
        case FaceDir::West:  return FaceDir::East;
        default:             return FaceDir::North;
    }
}

/// Stairs use facing = player look (ascent / full side away from player).
/// Most other horizontal-facing blocks (furnace, chest, …) face the player.
bool FacingMatchesPlayerLook(const std::string& typeName) {
    return typeName.size() >= 7 && typeName.compare(typeName.size() - 7, 7, "_stairs") == 0;
}

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

    const FaceDir placedFacing = FacingMatchesPlayerLook(type.name)
        ? *faceDir
        : OppositeHorizontalFaceDir(*faceDir);
    const char* facingValue = FaceDirToString(placedFacing);

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

/// Minecraft-like half selection from the targeted face / local hit Y.
uint32_t ApplyPlayerHalf(
    const blockstate::BlockStateRegistry& bsr,
    uint32_t stateId,
    const std::optional<blockplacement::HitPlacementInfo>& hitInfo
) {
    if (!hitInfo || !bsr.IsValidState(stateId)) {
        return stateId;
    }

    const uint16_t typeId = bsr.GetTypeIdFromState(stateId);
    const auto& type = bsr.GetType(typeId);

    const auto halfProperty = std::find_if(
        type.properties.begin(),
        type.properties.end(),
        [](const blockstate::BlockProperty& property) {
            return property.name == "half";
        }
    );
    if (halfProperty == type.properties.end()) {
        return stateId;
    }

    const char* halfValue = "bottom";
    switch (hitInfo->hitFace) {
        case FaceDir::Top:
            halfValue = "bottom";
            break;
        case FaceDir::Bottom:
            halfValue = "top";
            break;
        default:
            halfValue = (hitInfo->hitLocalY >= 0.5f) ? "top" : "bottom";
            break;
    }

    const auto& allowedValues = halfProperty->allowedValues;
    if (std::find(allowedValues.begin(), allowedValues.end(), halfValue) == allowedValues.end()) {
        return stateId;
    }

    return bsr.WithProperty(stateId, "half", halfValue);
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
        std::optional<FaceDir> faceDir,
        std::optional<HitPlacementInfo> hitInfo
    ) {
        if (keepStateId) {
            return stateId;
        }
    
        if (!bsr.IsValidState(stateId)) {
            return stateId;
        }

        stateId = ApplyPlayerFacing(bsr, stateId, faceDir);

        if (context == PlacementContext::PlayerPlacement) {
            stateId = ApplyPlayerHalf(bsr, stateId, hitInfo);
        }

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
        std::optional<FaceDir> faceDir,
        std::optional<HitPlacementInfo> hitInfo
    ) {
        return FinalizePlacedState(
            bsr, chunkManager, stateId, WorldCoord(x, y, z), context, keepStateId, faceDir, hitInfo
        );
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
