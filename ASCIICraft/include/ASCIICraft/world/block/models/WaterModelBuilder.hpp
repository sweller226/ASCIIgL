#pragma once

#include <ASCIICraft/world/block/models/BlockModel.hpp>

namespace blockmodels {
    struct WaterSpec {
        bool top = true;
    };

    blockstate::BlockModel BuildWaterModel(const WaterSpec& spec);
}