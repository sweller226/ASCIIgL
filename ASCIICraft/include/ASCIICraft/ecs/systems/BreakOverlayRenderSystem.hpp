#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <entt/entt.hpp>

namespace ASCIIgL {
class Material;
class Mesh;
}

namespace ecs::systems {

/// Draws the Minecraft-style destroy-stage crack overlay on the block currently
/// being mined (reads the player's MiningProgress, written by MiningSystem).
/// Geometry follows the baked BlockModel faces when available; otherwise a unit cube.
class BreakOverlayRenderSystem {
public:
    explicit BreakOverlayRenderSystem(entt::registry& registry);

    void Render();

private:
    static constexpr int kStageCount = 10;

    bool EnsureRenderResources();
    void ClearShapeCache();
    ASCIIgL::Mesh* ResolveOverlayMesh(uint32_t stateId, int stage);

    entt::registry& m_registry;
    bool m_renderResourcesReady = false;

    std::array<std::shared_ptr<ASCIIgL::Mesh>, kStageCount> m_stageMeshes{};
    std::array<float, kStageCount> m_stageLayers{};
    std::shared_ptr<ASCIIgL::Material> m_material;

    // Cached shape overlay for the current mining target.
    std::shared_ptr<ASCIIgL::Mesh> m_shapeMesh;
    uint32_t m_cachedStateId = 0;
    int m_cachedStage = -1;
};

} // namespace ecs::systems
