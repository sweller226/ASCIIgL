#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class BlockUpdateSystem : public ISystem {
public:
    explicit BlockUpdateSystem(entt::registry &registry, ASCIIgL::EventBus& eventBus);
    
    void Update() override;

private:
    entt::registry &m_registry;
    ASCIIgL::EventBus &eventBus;

    void BreakBlockEvents();
    void PlaceBlockEvents();
};

}

