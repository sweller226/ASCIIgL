// PlayerSaveSystem: the timer, the capture, and the refusals.
//
// The registry is built by hand rather than through PlayerFactory, which reaches for
// Screen::GetInst() and the ItemRegistry and so cannot run headless. What matters here
// is which components are read and when a write is allowed to happen.

#include <doctest/doctest.h>

#include "support/TempDir.hpp"

#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/systems/PlayerSaveSystem.hpp>
#include <ASCIICraft/save/PlayerDataStore.hpp>

#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

/// Hand-wound clock, so the save interval is reachable without waiting on it.
struct FakeClock {
    uint32_t seconds = 1000;
    std::function<uint32_t()> Fn() {
        return [this]() { return seconds; };
    }
};

entt::entity MakePlayer(entt::registry& reg,
                        const glm::vec3& position = glm::vec3(1.0f, 2.0f, 3.0f),
                        float yaw = 30.0f,
                        float pitch = -15.0f,
                        GameMode mode = GameMode::Creative) {
    const entt::entity e = reg.create();
    reg.emplace<ecs::components::PlayerTag>(e);

    auto& t = reg.emplace<ecs::components::Transform>(e);
    t.position = position;

    auto& cam = reg.emplace<ecs::components::PlayerCamera>(e);
    cam.camera.setCamDir(yaw, pitch);

    auto& pm = reg.emplace<ecs::components::PlayerMode>(e);
    pm.gamemode = mode;
    return e;
}

} // namespace

TEST_SUITE("ecs.tier1.playersavesystem") {

TEST_CASE("SaveNow writes what the player components hold") {
    testsupport::TempDir dir("savenow");
    entt::registry reg;
    MakePlayer(reg, glm::vec3(10.0f, 64.0f, -20.0f), 45.0f, -30.0f, GameMode::Spectator);

    save::PlayerDataStore store(dir / "player_data.json");
    ecs::systems::PlayerSaveSystem system(reg, store);

    REQUIRE(system.SaveNow());

    const auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.x == doctest::Approx(10.0f));
    CHECK(out->position.y == doctest::Approx(64.0f));
    CHECK(out->position.z == doctest::Approx(-20.0f));
    CHECK(out->yawDegrees == doctest::Approx(45.0f));
    CHECK(out->pitchDegrees == doctest::Approx(-30.0f));
    CHECK(out->gameMode == GameMode::Spectator);
}

TEST_CASE("capture reads position rather than the interpolated render position") {
    // renderPosition sits between fixed steps, so saving it would drift the player by
    // up to one step every time they quit.
    testsupport::TempDir dir("renderpos");
    entt::registry reg;
    const entt::entity player = MakePlayer(reg);
    auto& t = reg.get<ecs::components::Transform>(player);
    t.position       = glm::vec3(5.0f, 5.0f, 5.0f);
    t.renderPosition = glm::vec3(99.0f, 99.0f, 99.0f);

    save::PlayerDataStore store(dir / "player_data.json");
    ecs::systems::PlayerSaveSystem system(reg, store);
    REQUIRE(system.SaveNow());

    const auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.x == doctest::Approx(5.0f));
}

TEST_CASE("no player entity means no write") {
    testsupport::TempDir dir("noplayer");
    entt::registry reg;   // deliberately empty

    save::PlayerDataStore store(dir / "player_data.json");
    ecs::systems::PlayerSaveSystem system(reg, store);

    CHECK_FALSE(system.SaveNow());
    CHECK_FALSE(store.Exists());
}

TEST_CASE("a half-built player leaves an existing save untouched") {
    // The property that matters: a startup that failed partway must never replace a
    // good save with defaults.
    testsupport::TempDir dir("partial");
    const fs::path path = dir / "player_data.json";

    save::PlayerSaveData good;
    good.position = glm::vec3(123.0f, 45.0f, 67.0f);
    save::PlayerDataStore store(path);
    REQUIRE(store.Save(good));

    entt::registry reg;
    const entt::entity e = reg.create();
    reg.emplace<ecs::components::PlayerTag>(e);
    reg.emplace<ecs::components::Transform>(e);
    // No PlayerCamera, no PlayerMode.

    ecs::systems::PlayerSaveSystem system(reg, store);
    CHECK_FALSE(system.SaveNow());

    const auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.x == doctest::Approx(123.0f));
}

TEST_CASE("the first update establishes the baseline without writing") {
    testsupport::TempDir dir("firsttick");
    entt::registry reg;
    MakePlayer(reg);

    save::PlayerDataStore store(dir / "player_data.json");
    FakeClock clock;
    ecs::systems::PlayerSaveSystem system(reg, store, clock.Fn());

    system.Update();
    CHECK_FALSE(store.Exists());
}

TEST_CASE("no write before the interval elapses") {
    testsupport::TempDir dir("early");
    entt::registry reg;
    MakePlayer(reg);

    save::PlayerDataStore store(dir / "player_data.json");
    FakeClock clock;
    ecs::systems::PlayerSaveSystem system(reg, store, clock.Fn());

    system.Update();                                                    // baseline
    clock.seconds += ecs::systems::PlayerSaveSystem::SAVE_INTERVAL_SECONDS - 1;
    system.Update();

    CHECK_FALSE(store.Exists());
}

TEST_CASE("a write happens once the interval elapses") {
    testsupport::TempDir dir("ontime");
    entt::registry reg;
    MakePlayer(reg, glm::vec3(7.0f, 8.0f, 9.0f));

    save::PlayerDataStore store(dir / "player_data.json");
    FakeClock clock;
    ecs::systems::PlayerSaveSystem system(reg, store, clock.Fn());

    system.Update();                                                    // baseline
    clock.seconds += ecs::systems::PlayerSaveSystem::SAVE_INTERVAL_SECONDS;
    system.Update();

    REQUIRE(store.Exists());
    const auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.y == doctest::Approx(8.0f));
}

TEST_CASE("the interval restarts after each write") {
    testsupport::TempDir dir("restart");
    entt::registry reg;
    const entt::entity player = MakePlayer(reg, glm::vec3(1.0f, 1.0f, 1.0f));

    save::PlayerDataStore store(dir / "player_data.json");
    FakeClock clock;
    ecs::systems::PlayerSaveSystem system(reg, store, clock.Fn());

    system.Update();                                                    // baseline
    clock.seconds += ecs::systems::PlayerSaveSystem::SAVE_INTERVAL_SECONDS;
    system.Update();                                                    // writes

    // Move, then tick just short of another full interval: still the old position.
    reg.get<ecs::components::Transform>(player).position = glm::vec3(2.0f, 2.0f, 2.0f);
    clock.seconds += ecs::systems::PlayerSaveSystem::SAVE_INTERVAL_SECONDS - 1;
    system.Update();

    auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.x == doctest::Approx(1.0f));

    // One more second crosses it.
    clock.seconds += 1;
    system.Update();

    out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.x == doctest::Approx(2.0f));
}

TEST_CASE("SaveNow works without any prior update") {
    // Game::Shutdown calls it directly, and a session can end before the first
    // interval ever elapses.
    testsupport::TempDir dir("shutdown");
    entt::registry reg;
    MakePlayer(reg, glm::vec3(4.0f, 5.0f, 6.0f));

    save::PlayerDataStore store(dir / "player_data.json");
    FakeClock clock;
    ecs::systems::PlayerSaveSystem system(reg, store, clock.Fn());

    REQUIRE(system.SaveNow());
    const auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.z == doctest::Approx(6.0f));
}

TEST_CASE("a saved player round trips back through the spawn state") {
    // Closes the loop with Phase 3: what the system writes is what PlayerFactory reads.
    testsupport::TempDir dir("roundtrip");
    entt::registry reg;
    MakePlayer(reg, glm::vec3(-12.0f, 70.0f, 33.0f), -120.0f, 42.0f, GameMode::Survival);

    save::PlayerDataStore store(dir / "player_data.json");
    ecs::systems::PlayerSaveSystem system(reg, store);
    REQUIRE(system.SaveNow());

    const auto loaded = store.Load();
    REQUIRE(loaded.has_value());
    const ecs::factories::PlayerSpawnState spawn = save::ToSpawnState(*loaded);

    CHECK(spawn.position.x == doctest::Approx(-12.0f));
    CHECK(spawn.position.y == doctest::Approx(70.0f));
    CHECK(spawn.position.z == doctest::Approx(33.0f));
    CHECK(spawn.yawDegrees == doctest::Approx(-120.0f));
    CHECK(spawn.pitchDegrees == doctest::Approx(42.0f));
    CHECK(spawn.mode == GameMode::Survival);
}

} // TEST_SUITE("ecs.tier1.playersavesystem")
