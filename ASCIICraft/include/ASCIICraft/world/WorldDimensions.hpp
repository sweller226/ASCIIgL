#pragma once

struct WorldDimensions {
    unsigned int XZ_CHUNK_LIMIT;
    int Y_MIN_CHUNK_LIMIT;
    int Y_MAX_CHUNK_LIMIT;

    WorldDimensions(unsigned int xzLimit, int yMin, int yMax)
        : XZ_CHUNK_LIMIT(xzLimit), Y_MIN_CHUNK_LIMIT(yMin), Y_MAX_CHUNK_LIMIT(yMax) {}
};