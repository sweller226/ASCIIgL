#include <ASCIICraft/save/PlayerDataStore.hpp>

#include <ASCIICraft/save/PlayerDataJson.hpp>
#include <ASCIICraft/util/JsonUtil.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace save {

PlayerDataStore::PlayerDataStore(fs::path filePath) : filePath_(std::move(filePath)) {}

bool PlayerDataStore::Exists() const {
    std::error_code ec;
    return fs::is_regular_file(filePath_, ec);
}

std::optional<PlayerSaveData> PlayerDataStore::Load() const {
    const std::string path = filePath_.string();

    if (!Exists()) {
        ASCIIgL::Logger::Debugf("No player data at '%s'; using the world spawn point.", path.c_str());
        return std::nullopt;
    }

    const auto text = jsonutil::ReadFileText(path);
    if (!text.Ok()) {
        ASCIIgL::Logger::Warningf("Could not read player data: %s", text.error->c_str());
        return std::nullopt;
    }

    const auto parsed = ParsePlayerData(*text.value, path);
    if (!parsed.Ok()) {
        ASCIIgL::Logger::Warningf("Ignoring player data: %s", parsed.error->c_str());
        return std::nullopt;
    }

    return *parsed.value;
}

bool PlayerDataStore::Save(const PlayerSaveData& data) const {
    const std::string path = filePath_.string();

    if (!IsFinite(data)) {
        // nlohmann writes non-finite floats as null, which this file's own parser then
        // rejects - so writing would trade a good save for an unloadable one.
        ASCIIgL::Logger::Errorf("Refusing to write non-finite player data to '%s'.", path.c_str());
        return false;
    }

    std::error_code ec;
    const fs::path parent = filePath_.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            ASCIIgL::Logger::Errorf("Could not create '%s' for player data: %s",
                                    parent.string().c_str(), ec.message().c_str());
            return false;
        }
    }

    fs::path tempPath = filePath_;
    tempPath += ".tmp";

    {
        std::ofstream out(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            ASCIIgL::Logger::Errorf("Could not open '%s' for writing.", tempPath.string().c_str());
            return false;
        }
        out << PlayerDataToJson(data).dump(2) << '\n';
        out.flush();
        if (!out.good()) {
            ASCIIgL::Logger::Errorf("Failed while writing '%s'.", tempPath.string().c_str());
            out.close();
            std::error_code removeEc;
            fs::remove(tempPath, removeEc);
            return false;
        }
    }

    fs::rename(tempPath, filePath_, ec);
    if (ec) {
        // Windows will not rename onto an existing file, so drop the target and retry.
        std::error_code removeEc;
        fs::remove(filePath_, removeEc);
        ec.clear();
        fs::rename(tempPath, filePath_, ec);
    }

    if (ec) {
        ASCIIgL::Logger::Errorf("Could not move '%s' onto '%s': %s",
                                tempPath.string().c_str(), path.c_str(), ec.message().c_str());
        std::error_code removeEc;
        fs::remove(tempPath, removeEc);
        return false;
    }

    return true;
}

} // namespace save
