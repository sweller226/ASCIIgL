#pragma once

#include <array>
#include <memory>

#include <entt/entt.hpp>

namespace ASCIIgL {
class Material;
class Mesh;
}

namespace ecs::systems {

/// Draws the Minecraft-style destroy-stage crack overlay on the block currently
/// being mined (reads the player's MiningProgress, written by MiningSystem).
class BreakOverlayRenderSystem {
public:
    explicit BreakOverlayRenderSystem(entt::registry& registry);

    void Render();

private:
    static constexpr int kStageCount = 10;

    bool EnsureRenderResources();

    entt::registry& m_registry;
    bool m_renderResourcesReady = false;

    std::array<std::shared_ptr<ASCIIgL::Mesh>, kStageCount> m_stageMeshes{};
    std::shared_ptr<ASCIIgL::Material> m_material;
};

} // namespace ecs::systems
