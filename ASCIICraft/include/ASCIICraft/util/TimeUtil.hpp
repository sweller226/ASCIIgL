#pragma once
#include <chrono>
#include <cstdint>

namespace util {
    inline uint32_t NowSeconds() {
        using namespace std::chrono;
        return duration_cast<seconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }
}