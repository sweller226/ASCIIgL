#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace textures {

struct CatalogEntry {
    const char* textureId; // e.g. "minecraft:blocks/stone"
    const char* filePath;  // game-local path used by LoadTextureArray
};

std::vector<std::string> BuildTexturePaths(const std::vector<CatalogEntry>& catalog);

std::unordered_map<std::string, int> BuildTextureIdToLayerMap(const std::vector<CatalogEntry>& catalog);

std::string CanonicalizeTextureId(const std::string& textureId);

/// Returns -1 when missing.
int GetLayerForTextureId(
    const std::vector<CatalogEntry>& catalog,
    const std::string& textureId
);

} // namespace textures
