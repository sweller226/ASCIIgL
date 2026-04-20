#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace blocktextures {

    struct CatalogEntry {
        const char* textureId; // e.g. "minecraft:blocks/stone"
        const char* filePath;  // game-local path used by LoadTextureArray
    };

    const std::vector<CatalogEntry>& GetBlockTextureCatalog();
    std::vector<std::string> BuildBlockTexturePaths();
    std::unordered_map<std::string, int> BuildTextureIdToLayerMap();
    std::string CanonicalizeTextureId(const std::string& textureId);

    // Returns -1 when missing.
    int GetLayerForTextureId(const std::string& textureId);

} // namespace blocktextures

