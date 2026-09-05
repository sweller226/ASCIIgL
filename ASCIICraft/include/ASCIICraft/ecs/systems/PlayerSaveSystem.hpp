#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/save/PlayerDataStore.hpp>
#include <ASCIICraft/save/PlayerSaveData.hpp>

namespace ecs::systems {

/// Persists the player's position, facing and game mode to world/player_data.json.
///
/// Saving only on shutdown would mean a crash leaves a saved world next to a
/// spawn-point player - the two halves of one save out of step. A short interval keeps
/// the loss window small; the write is a couple of hundred bytes.
///
/// Holds no captured state: Update() reads the registry only when it is about to write,
/// so a frame costs a clock read and a comparison.
class PlayerSaveSystem : public ISystem {
public:
    /// \param nowSeconds monotonic clock; null uses util::NowSeconds. Injected so a
    ///        test can reach the interval without actually waiting, mirroring
    ///        ChunkManagerDeps::nowSeconds.
    PlayerSaveSystem(entt::registry& registry,
                     save::PlayerDataStore& store,
                     std::function<uint32_t()> nowSeconds = nullptr);

    /// Writes the player if SAVE_INTERVAL_SECONDS has elapsed since the last write.
    void Update() override;

    /// Captures and writes immediately. Called from Game::Shutdown.
    /// \return false if there is no player to capture or the write failed.
    bool SaveNow();

    static constexpr uint32_t SAVE_INTERVAL_SECONDS = 10;

private:
    /// nullopt when the player entity or any component it needs is missing - so a
    /// failed startup can never overwrite a good save with defaults.
    std::optional<save::PlayerSaveData> Capture() const;

    entt::registry& m_registry;
    save::PlayerDataStore& m_store;
    std::function<uint32_t()> m_nowSeconds;
    uint32_t m_lastSaveSeconds = 0;
};

} // namespace ecs::systems
