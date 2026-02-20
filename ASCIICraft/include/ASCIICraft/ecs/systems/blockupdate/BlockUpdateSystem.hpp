#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class BlockUpdateSystem : public ISystem {
public:
    explicit BlockUpdateSystem(entt::registry &registry, EventBus& eventBus);
    
    void Update() override;

private:
    entt::registry &m_registry;
    EventBus &eventBus;

    void BreakBlockEvents();
    void PlaceBlockEvents();
};

}

