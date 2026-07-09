#pragma once

#include <ASCIIgL/engine/MonochromeMapping.hpp>

#include <glm/vec3.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace textures {

struct CatalogEntry {
    const char* textureId; // e.g. "minecraft:blocks/stone"
    const char* filePath;  // game-local path used by LoadTextureArray
    const glm::ivec3* monoHueOverride = nullptr; // nullptr = use global default hue
    float paletteWeight = 1.0f; // palette clustering influence (0 = skip)
};

/// Catalog vector index becomes the texture-array layer baked into block model vertices.
/// Do not remove or reorder entries once shipped; only append new textures at the end.

std::vector<std::string> BuildTexturePaths(const std::vector<CatalogEntry>& catalog);

std::vector<ASCIIgL::MonochromeMapping> BuildPerTileMonochromeMappings(
    const std::vector<CatalogEntry>& catalog,
    const ASCIIgL::MonochromeMapping& defaultMono);

std::vector<float> BuildPaletteLayerWeights(const std::vector<CatalogEntry>& catalog);

std::unordered_map<std::string, int> BuildTextureIdToLayerMap(const std::vector<CatalogEntry>& catalog);

std::string CanonicalizeTextureId(const std::string& textureId);

/// Returns -1 when missing.
int GetLayerForTextureId(
    const std::vector<CatalogEntry>& catalog,
    const std::string& textureId
);

} // namespace textures
