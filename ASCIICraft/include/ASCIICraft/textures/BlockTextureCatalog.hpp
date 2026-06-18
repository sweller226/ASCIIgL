#pragma once

#include <ASCIICraft/textures/TextureCatalog.hpp>

#include <string>
#include <vector>

namespace blocktextures {

using CatalogEntry = textures::CatalogEntry;

const std::vector<CatalogEntry>& GetBlockTextureCatalog();

} // namespace blocktextures
