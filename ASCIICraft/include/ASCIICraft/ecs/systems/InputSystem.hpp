#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/input/IGameInputSource.hpp>

class EventBus;

namespace ecs::systems {

/// Who is consuming input this frame: gameplay (movement, look, jump, break, place, quit) or GUI.
enum class InputMode {
    Gameplay,
    GUI,
};

/// Central input. Calls InputManager::Update() once per frame, emits discrete input events,
/// and exposes action state (always pass-through from engine).
///
/// When mode is GUI, Update() does not emit gameplay events and Game should skip movement/camera.
/// Other input (e.g. inventory toggle) is unaffected.
class InputSystem : public ISystem, public ASCIICraft::IGameInputSource {
public:
    InputSystem(entt::registry& registry, EventBus& eventBus);

    void Update() override;

    void SetInputMode(InputMode mode) { m_inputMode = mode; }
    InputMode GetInputMode() const { return m_inputMode; }

    // IGameInputSource — always reports engine state (pass-through)
    bool IsActionHeld(const std::string& action) const override;
    bool IsActionPressed(const std::string& action) const override;
    float GetMouseSensitivity() const override;

private:
    entt::registry& m_registry;
    EventBus& m_eventBus;
    InputMode m_inputMode = InputMode::Gameplay;
};

} // namespace ecs::systems
