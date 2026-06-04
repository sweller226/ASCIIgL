#pragma once

#include <ASCIICraft/textures/TextureCatalog.hpp>

#include <string>
#include <vector>

namespace itemtextures {

using CatalogEntry = textures::CatalogEntry;

const std::vector<CatalogEntry>& GetItemTextureCatalog();

} // namespace itemtextures
