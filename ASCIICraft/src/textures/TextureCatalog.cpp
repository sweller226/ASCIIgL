#include <ASCIICraft/textures/TextureCatalog.hpp>

namespace textures {

std::vector<std::string> BuildTexturePaths(const std::vector<CatalogEntry>& catalog) {
    std::vector<std::string> out;
    out.reserve(catalog.size());
    for (const auto& e : catalog) {
        out.emplace_back(e.filePath);
    }
    return out;
}

std::unordered_map<std::string, int> BuildTextureIdToLayerMap(const std::vector<CatalogEntry>& catalog) {
    std::unordered_map<std::string, int> out;
    out.reserve(catalog.size());
    for (size_t i = 0; i < catalog.size(); ++i) {
        out.emplace(catalog[i].textureId, static_cast<int>(i));
    }
    return out;
}

std::string CanonicalizeTextureId(const std::string& textureId) {
    if (textureId.find(':') != std::string::npos) {
        return textureId;
    }
    return "minecraft:" + textureId;
}

int GetLayerForTextureId(
    const std::vector<CatalogEntry>& catalog,
    const std::string& textureId
) {
    static const std::vector<CatalogEntry>* cachedCatalog = nullptr;
    static std::unordered_map<std::string, int> layerMap;

    if (cachedCatalog != &catalog) {
        cachedCatalog = &catalog;
        layerMap = BuildTextureIdToLayerMap(catalog);
    }

    auto it = layerMap.find(CanonicalizeTextureId(textureId));
    if (it == layerMap.end()) {
        return -1;
    }
    return it->second;
}

} // namespace textures
