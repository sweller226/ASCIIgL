#include <ASCIICraft/ecs/systems/PlayerSaveSystem.hpp>

#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/util/TimeUtil.hpp>

namespace ecs::systems {

PlayerSaveSystem::PlayerSaveSystem(entt::registry& registry,
                                   save::PlayerDataStore& store,
                                   std::function<uint32_t()> nowSeconds)
    : m_registry(registry)
    , m_store(store)
    , m_nowSeconds(nowSeconds ? std::move(nowSeconds) : util::NowSeconds) {}

std::optional<save::PlayerSaveData> PlayerSaveSystem::Capture() const {
    const entt::entity player = components::GetPlayerEntity(m_registry);
    if (player == entt::null) {
        return std::nullopt;
    }

    const auto* transform = m_registry.try_get<components::Transform>(player);
    const auto* camera    = m_registry.try_get<components::PlayerCamera>(player);
    const auto* mode      = m_registry.try_get<components::PlayerMode>(player);
    if (!transform || !camera || !mode) {
        return std::nullopt;
    }

    save::PlayerSaveData data;
    // position, not renderPosition: the latter is the interpolated display position and
    // sits between fixed steps.
    data.position     = transform->position;
    data.yawDegrees   = camera->camera.GetYaw();
    data.pitchDegrees = camera->camera.GetPitch();
    data.gameMode     = mode->gamemode;
    return data;
}

bool PlayerSaveSystem::SaveNow() {
    const auto data = Capture();
    return data && m_store.Save(*data);
}

void PlayerSaveSystem::Update() {
    const uint32_t now = m_nowSeconds();

    // Same first-tick guard as ChunkManager's autosave: the first Update establishes
    // the baseline rather than writing, so startup does not pay for a save.
    if (m_lastSaveSeconds == 0) {
        m_lastSaveSeconds = now;
        return;
    }

    if (now - m_lastSaveSeconds >= SAVE_INTERVAL_SECONDS) {
        SaveNow();
        m_lastSaveSeconds = now;
    }
}

} // namespace ecs::systems
