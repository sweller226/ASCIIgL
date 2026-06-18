#pragma once

#include <memory>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

namespace ASCIIgL {
class Material;
class Mesh;
}

namespace ecs::systems {

class BlockTargetSystem : public ISystem {
public:
    explicit BlockTargetSystem(entt::registry& registry);

    void Update() override;
    void Render();

    void SetGameplayActive(bool gameplayActive);

private:
    bool EnsureRenderResources();

    entt::registry& m_registry;
    bool m_gameplayActive = true;
    bool m_renderResourcesReady = false;

    std::shared_ptr<ASCIIgL::Mesh> m_outlineMesh;
    std::shared_ptr<ASCIIgL::Material> m_outlineMaterial;
};

} // namespace ecs::systems
