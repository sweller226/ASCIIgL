#include <ASCIICraft/save/SavePaths.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <system_error>

namespace fs = std::filesystem;

namespace save {

fs::path MigrateLegacySaveLayout(const fs::path& worldRoot, const fs::path& legacyRegionDir) {
    const fs::path target = worldRoot / kRegionsSubdirName;
    std::error_code ec;

    if (!fs::is_directory(legacyRegionDir, ec)) {
        // The overwhelmingly common case: nothing to migrate.
        return target;
    }

    if (fs::exists(worldRoot, ec)) {
        ASCIIgL::Logger::Warningf(
            "Found a legacy save at '%s' next to an existing '%s'. Loading '%s' and leaving "
            "both on disk - move the legacy one in manually if it holds the world you want.",
            legacyRegionDir.string().c_str(), worldRoot.string().c_str(), target.string().c_str());
        return target;
    }

    fs::create_directories(worldRoot, ec);
    if (ec) {
        ASCIIgL::Logger::Errorf("Save migration: could not create '%s': %s. Loading '%s' instead.",
                                worldRoot.string().c_str(), ec.message().c_str(),
                                legacyRegionDir.string().c_str());
        return legacyRegionDir;
    }

    fs::rename(legacyRegionDir, target, ec);
    if (ec) {
        ASCIIgL::Logger::Errorf("Save migration: could not move '%s' to '%s': %s. Loading '%s' "
                                "instead; migration will retry on the next launch.",
                                legacyRegionDir.string().c_str(), target.string().c_str(),
                                ec.message().c_str(), legacyRegionDir.string().c_str());

        // Drop the root we just made, but only if it is still empty. Leaving it behind
        // would trip the "world root already exists" guard above and block the retry on
        // every future launch, stranding this save for good.
        std::error_code removeEc;
        fs::remove(worldRoot, removeEc);

        // Keep playing the world the player actually has. Falling through to the world
        // root here would silently generate a fresh empty one in its place.
        return legacyRegionDir;
    }

    ASCIIgL::Logger::Infof("Migrated legacy save: '%s' -> '%s'",
                           legacyRegionDir.string().c_str(), target.string().c_str());
    return target;
}

} // namespace save
