// ecs/systems/ISystem.hpp
#pragma once

namespace ecs::systems {

/// Base interface for all ECS systems
/// Provides a common Update() method that all systems must implement
class ISystem {
public:
    virtual ~ISystem() = default;
    
    /// Update method called every frame
    /// Systems implement their logic here
    virtual void Update() = 0;

protected:
    // Systems should not be copyable
    ISystem() = default;
    ISystem(const ISystem&) = delete;
    ISystem& operator=(const ISystem&) = delete;
    ISystem(ISystem&&) = default;
    ISystem& operator=(ISystem&&) = default;
};

} // namespace ecs::systems
