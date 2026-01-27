#include <ASCIICraft/events/EventBus.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class BlockUpdateSystem {
public:
    explicit BlockUpdateSystem(entt::registry &registry, EventBus& eventBus) noexcept;
    BlockUpdateSystem(const BlockUpdateSystem&) = delete;
    BlockUpdateSystem& operator=(const BlockUpdateSystem&) = delete;

    void Update();

private:
    entt::registry &m_registry;
    EventBus &eventBus;

    void BreakBlockEvents();
    void PlaceBlockEvents();
};

}

