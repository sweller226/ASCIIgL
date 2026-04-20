#pragma once

#include <string>

namespace blockstate {

class BlockStateRegistry;
class JsonBlockStateLoader;

}

namespace blockmodels {

class BlockModelLibrary;
class JsonModelLoader;

}

namespace blockstate {

/// Loads blockstate JSON via \p blockstateLoader, resolves variant → model → bake, and registers one model-set per
/// \c stateId for that type (single entry for object variants, multi-entry for array variants).
/// Call after \ref BlockStateRegistry::RegisterType and \ref SetDerivedData for the type.
/// Returns \c false if the blockstate failed to load, the type name is unknown, or any state could not be resolved/baked.
///
/// Variant lookup: exact \ref BuildVariantKey, then \c "" and \c "normal" (1.8.9 compatibility) are tried in order.
bool RegisterJsonBackedBlockType(
    const std::string& typeName,
    BlockStateRegistry& bsr,
    blockmodels::BlockModelLibrary& modelLibrary,
    JsonBlockStateLoader& blockstateLoader,
    blockmodels::JsonModelLoader& modelLoader
);

} // namespace blockstate
