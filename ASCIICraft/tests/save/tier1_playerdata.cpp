// player_data.json round-trip, validation, and atomic write.
//
// The point of these tests is the failure behaviour. A corrupt or hand-mangled player
// file must cost the player their position and nothing else - never a crash, never a
// NaN reaching the ECS, never a good save replaced by an unloadable one.

#include <doctest/doctest.h>

#include "support/TempDir.hpp"

#include <ASCIICraft/save/PlayerDataJson.hpp>
#include <ASCIICraft/save/PlayerDataStore.hpp>
#include <ASCIICraft/save/PlayerSaveData.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace fs = std::filesystem;

namespace {

save::PlayerSaveData MakeData() {
    save::PlayerSaveData d;
    d.position     = glm::vec3(12.5f, 90.0f, -33.25f);
    d.yawDegrees   = -45.0f;
    d.pitchDegrees = 12.5f;
    d.gameMode     = GameMode::Creative;
    return d;
}

void WriteText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

TEST_SUITE("save.tier1.playerdata") {

TEST_CASE("data survives a round trip through json") {
    const save::PlayerSaveData in = MakeData();
    const auto out = save::PlayerDataFromJson(save::PlayerDataToJson(in), "roundtrip");

    REQUIRE(out.Ok());
    CHECK(out.value->position.x == doctest::Approx(in.position.x));
    CHECK(out.value->position.y == doctest::Approx(in.position.y));
    CHECK(out.value->position.z == doctest::Approx(in.position.z));
    CHECK(out.value->yawDegrees == doctest::Approx(in.yawDegrees));
    CHECK(out.value->pitchDegrees == doctest::Approx(in.pitchDegrees));
    CHECK(out.value->gameMode == in.gameMode);
}

TEST_CASE("every game mode round trips by name") {
    for (const GameMode mode : {GameMode::Survival, GameMode::Creative, GameMode::Spectator}) {
        save::PlayerSaveData in = MakeData();
        in.gameMode = mode;

        const nlohmann::json j = save::PlayerDataToJson(in);
        CHECK(j.at("gamemode").get<std::string>() == save::GameModeToString(mode));

        const auto out = save::PlayerDataFromJson(j, "modes");
        REQUIRE(out.Ok());
        CHECK(out.value->gameMode == mode);
    }
}

TEST_CASE("unknown keys are ignored") {
    // The forward-compatible half of the version field: a newer build may add fields.
    nlohmann::json j = save::PlayerDataToJson(MakeData());
    j["dimension"] = "the_nether";
    j["health"] = 17;

    CHECK(save::PlayerDataFromJson(j, "unknown").Ok());
}

// --- structural rejections: the whole file is discarded --------------------------

TEST_CASE("a non-object document is rejected") {
    CHECK_FALSE(save::ParsePlayerData("[1, 2, 3]", "arr").Ok());
    CHECK_FALSE(save::ParsePlayerData("\"hello\"", "str").Ok());
}

TEST_CASE("a missing or non-integer version is rejected") {
    nlohmann::json j = save::PlayerDataToJson(MakeData());
    j.erase("version");
    CHECK_FALSE(save::PlayerDataFromJson(j, "noversion").Ok());

    j = save::PlayerDataToJson(MakeData());
    j["version"] = "one";
    CHECK_FALSE(save::PlayerDataFromJson(j, "strversion").Ok());
}

TEST_CASE("a newer version is rejected rather than guessed at") {
    nlohmann::json j = save::PlayerDataToJson(MakeData());
    j["version"] = save::kPlayerDataVersion + 1;

    const auto out = save::PlayerDataFromJson(j, "newer");
    REQUIRE_FALSE(out.Ok());
    CHECK(out.error->find("newer build") != std::string::npos);
}

TEST_CASE("a missing or malformed position is rejected") {
    nlohmann::json j = save::PlayerDataToJson(MakeData());
    j.erase("position");
    CHECK_FALSE(save::PlayerDataFromJson(j, "nopos").Ok());

    j = save::PlayerDataToJson(MakeData());
    j["position"] = {1.0, 2.0};                     // wrong arity
    CHECK_FALSE(save::PlayerDataFromJson(j, "arity").Ok());

    j = save::PlayerDataToJson(MakeData());
    j["position"] = {0.0, nullptr, 0.0};            // null component
    CHECK_FALSE(save::PlayerDataFromJson(j, "nullcomp").Ok());
}

TEST_CASE("an overflowing number in the file is rejected") {
    // JSON has no Infinity literal, so this is how one would try to smuggle it in.
    // nlohmann's parser rejects the overflow outright, before any of our validation
    // runs - which is why the isfinite gate below is tested separately.
    CHECK_FALSE(save::ParsePlayerData(
        R"({"version":1,"position":[1e400,90.0,0.0],"yaw":0.0,"pitch":0.0})", "overflow").Ok());
}

TEST_CASE("a non-finite position is rejected by the isfinite gate") {
    // The gate that matters, exercised directly: a NaN or infinity reaching
    // Transform::position poisons chunk-streaming maths and the view matrix, and
    // Camera3D's clamp is comparison-based so it lets NaN straight through.
    // json holds non-finite doubles happily in memory - it only balks when dumping -
    // so this is reachable by any caller that builds the json itself.
    for (const float bad : {std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity()}) {
        nlohmann::json j = save::PlayerDataToJson(MakeData());
        j["position"] = {0.0f, bad, 0.0f};
        CHECK_FALSE(save::PlayerDataFromJson(j, "nonfinitepos").Ok());
    }
}

TEST_CASE("a non-finite angle is rejected by the isfinite gate") {
    for (const char* key : {"yaw", "pitch"}) {
        nlohmann::json j = save::PlayerDataToJson(MakeData());
        j[key] = std::numeric_limits<float>::quiet_NaN();
        CHECK_FALSE(save::PlayerDataFromJson(j, "nonfiniteangle").Ok());
    }
}

TEST_CASE("an absurd coordinate is rejected") {
    nlohmann::json j = save::PlayerDataToJson(MakeData());
    j["position"] = {save::kMaxAbsCoordinate * 10.0f, 90.0f, 0.0f};
    CHECK_FALSE(save::PlayerDataFromJson(j, "huge").Ok());
}

TEST_CASE("a malformed angle in the file is rejected") {
    // Overflow is caught by the parser; null is caught by our own type check.
    CHECK_FALSE(save::ParsePlayerData(
        R"({"version":1,"position":[0,90,0],"pitch":1e400})", "infpitch").Ok());
    CHECK_FALSE(save::ParsePlayerData(
        R"({"version":1,"position":[0,90,0],"yaw":null})", "nullyaw").Ok());
}

TEST_CASE("truncated json is rejected") {
    CHECK_FALSE(save::ParsePlayerData(R"({"version":1,"position":[0,90)", "trunc").Ok());
    CHECK_FALSE(save::ParsePlayerData("", "empty").Ok());
}

// --- soft errors: one field defaults and the rest is honoured ---------------------

TEST_CASE("absent angles fall back to the spawn defaults") {
    const auto out = save::ParsePlayerData(
        R"({"version":1,"position":[5.0,80.0,-5.0],"gamemode":"survival"})", "noangles");

    REQUIRE(out.Ok());
    CHECK(out.value->yawDegrees == doctest::Approx(ecs::factories::kDefaultSpawnYawDegrees));
    CHECK(out.value->pitchDegrees == doctest::Approx(ecs::factories::kDefaultSpawnPitchDegrees));
    CHECK(out.value->position.x == doctest::Approx(5.0f));
}

TEST_CASE("an unrecognised gamemode keeps the position and defaults to survival") {
    // A typo in one field must not cost the player where they were standing.
    const auto out = save::ParsePlayerData(
        R"({"version":1,"position":[5.0,80.0,-5.0],"gamemode":"nonsense"})", "badmode");

    REQUIRE(out.Ok());
    CHECK(out.value->gameMode == GameMode::Survival);
    CHECK(out.value->position.y == doctest::Approx(80.0f));
}

TEST_CASE("an out-of-range pitch is clamped rather than rejected") {
    const auto out = save::ParsePlayerData(
        R"({"version":1,"position":[0,90,0],"pitch":250.0})", "steeppitch");

    REQUIRE(out.Ok());
    CHECK(out.value->pitchDegrees == doctest::Approx(save::kPitchClampDegrees));
}

TEST_CASE("yaw is normalised into a readable range") {
    const auto out = save::ParsePlayerData(
        R"({"version":1,"position":[0,90,0],"yaw":720.0})", "spun");

    REQUIRE(out.Ok());
    CHECK(out.value->yawDegrees == doctest::Approx(0.0f));
    CHECK(out.value->yawDegrees >= -180.0f);
    CHECK(out.value->yawDegrees < 180.0f);
}

// --- the store ------------------------------------------------------------------

TEST_CASE("a store round trips through a real file") {
    testsupport::TempDir dir("store");
    const save::PlayerDataStore store(dir / "player_data.json");

    const save::PlayerSaveData in = MakeData();
    REQUIRE(store.Save(in));
    CHECK(store.Exists());

    const auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.z == doctest::Approx(in.position.z));
    CHECK(out->gameMode == in.gameMode);
}

TEST_CASE("loading a missing file is not an error") {
    testsupport::TempDir dir("absent");
    const save::PlayerDataStore store(dir / "player_data.json");

    CHECK_FALSE(store.Exists());
    CHECK_FALSE(store.Load().has_value());
}

TEST_CASE("a corrupt file loads as nullopt but is left on disk") {
    // Left alone deliberately: it is the only record of where the player was and may
    // still be salvageable by hand.
    testsupport::TempDir dir("corrupt");
    const fs::path path = dir / "player_data.json";
    WriteText(path, R"({"version":1,"position":[0,90)");

    const save::PlayerDataStore store(path);
    CHECK(store.Exists());
    CHECK_FALSE(store.Load().has_value());
    CHECK(fs::exists(path));
}

TEST_CASE("saving twice leaves one file and no temp behind") {
    testsupport::TempDir dir("twice");
    const fs::path path = dir / "player_data.json";
    const save::PlayerDataStore store(path);

    REQUIRE(store.Save(MakeData()));

    save::PlayerSaveData second = MakeData();
    second.position = glm::vec3(1.0f, 2.0f, 3.0f);
    REQUIRE(store.Save(second));

    CHECK_FALSE(fs::exists(dir / "player_data.json.tmp"));

    const auto out = store.Load();
    REQUIRE(out.has_value());
    CHECK(out->position.x == doctest::Approx(1.0f));
}

TEST_CASE("saving creates the directory when it does not exist yet") {
    testsupport::TempDir dir("mkdir");
    const save::PlayerDataStore store(dir / "world" / "player_data.json");

    REQUIRE(store.Save(MakeData()));
    CHECK(store.Load().has_value());
}

TEST_CASE("non-finite data is refused rather than overwriting a good save") {
    // Regression guard: a wedged physics step could hand Capture() a NaN. nlohmann
    // writes that as null, which this parser rejects - so the write must not happen.
    testsupport::TempDir dir("nonfinite");
    const fs::path path = dir / "player_data.json";
    const save::PlayerDataStore store(path);

    REQUIRE(store.Save(MakeData()));
    const std::string before = ReadText(path);

    save::PlayerSaveData bad = MakeData();
    bad.position.y = std::numeric_limits<float>::quiet_NaN();

    CHECK_FALSE(store.Save(bad));
    CHECK(ReadText(path) == before);
    CHECK(store.Load().has_value());
}

TEST_CASE("the written file is human readable") {
    // It is meant to be hand-editable, so pretty-printing and a named game mode are
    // part of the contract rather than incidental.
    testsupport::TempDir dir("readable");
    const fs::path path = dir / "player_data.json";
    const save::PlayerDataStore store(path);
    REQUIRE(store.Save(MakeData()));

    const std::string text = ReadText(path);
    CHECK(text.find('\n') != std::string::npos);
    CHECK(text.find("\"gamemode\": \"creative\"") != std::string::npos);
}

} // TEST_SUITE("save.tier1.playerdata")
