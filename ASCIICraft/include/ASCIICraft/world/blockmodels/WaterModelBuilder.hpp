#pragma once

#include <ASCIICraft/world/blockmodels/BlockModel.hpp>

namespace blockmodels {
    struct WaterSpec {
        bool top = true;
    };

    blockstate::BlockModel BuildWaterModel(const WaterSpec& spec);
}