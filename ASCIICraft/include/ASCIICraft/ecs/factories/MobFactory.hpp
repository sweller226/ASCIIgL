#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace ASCIIgL { class Texture; class Mesh; }

namespace ecs::factories {

class MobFactory {
public:
    explicit MobFactory(entt::registry& registry);
    ~MobFactory();

    // Build meshes and load textures for all mob types
    void init();

    // Spawn/despawn
    entt::entity spawnMob(uint32_t typeId, const glm::ivec3& pos);
    void despawnMob(entt::entity e);

    // Tick AI and clean up invalid entities
    void update(float dt);

    // Iterate active mobs
    template<typename Fn>
    void forEach(Fn&& fn) {
        for (auto e : m_active) fn(e);
    }

    // Access loaded textures (for creating per-type materials externally)
    const std::unordered_map<uint32_t, std::shared_ptr<ASCIIgL::Texture>>& getTextures() const { return m_textures; }
    const std::shared_ptr<ASCIIgL::Texture>& getSheepFurTexture() const { return m_sheepFurTexture; }

private:
    entt::registry& m_registry;
    std::vector<entt::entity> m_active;
    std::unordered_map<uint32_t, std::shared_ptr<ASCIIgL::Texture>> m_textures;
    std::unordered_map<uint32_t, std::shared_ptr<ASCIIgL::Mesh>> m_meshes;

    // Sheep fur mesh — rendered as overlay on top of base sheep mesh
    std::shared_ptr<ASCIIgL::Texture> m_sheepFurTexture;
    std::shared_ptr<ASCIIgL::Mesh> m_sheepFurMesh;
};

} // namespace ecs::factories
