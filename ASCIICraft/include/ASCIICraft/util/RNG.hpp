#pragma once

#include <random>

#include <glm/vec3.hpp>

namespace util {
    class RNG {
    public:
        RNG() : m_engine(std::random_device{}()) {}
        explicit RNG(uint32_t seed) : m_engine(seed) {}

        float NextFloat(float min = 0.0f, float max = 1.0f) {
            return std::uniform_real_distribution<float>(min, max)(m_engine);
        }

        int NextInt(int min, int max) {
            return std::uniform_int_distribution<int>(min, max)(m_engine);
        }

        glm::vec3 NextVec3(float min, float max) {
            return { NextFloat(min, max), NextFloat(min, max), NextFloat(min, max) };
        }

    private:
        std::mt19937 m_engine;
    };
}