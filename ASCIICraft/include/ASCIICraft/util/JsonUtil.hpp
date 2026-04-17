#pragma once

#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace jsonutil {

    template <typename T>
    struct LoadResult {
        std::optional<T> value;
        std::optional<std::string> error;

        bool Ok() const { return value.has_value(); }
        static LoadResult<T> Success(T inValue) {
            LoadResult<T> out;
            out.value = std::move(inValue);
            return out;
        }
        static LoadResult<T> Failure(std::string message) {
            LoadResult<T> out;
            out.error = std::move(message);
            return out;
        }
    };

    template <typename T>
    LoadResult<T> Fail(std::string msg) {
        return LoadResult<T>::Failure(std::move(msg));
    }

    inline LoadResult<nlohmann::json> ParseJson(const std::string& text, const std::string& debugName) {
        try {
            return LoadResult<nlohmann::json>::Success(nlohmann::json::parse(text));
        } catch (const std::exception& e) {
            return Fail<nlohmann::json>(debugName + ": json parse error: " + std::string(e.what()));
        }
    }

    inline LoadResult<std::string> ReadFileText(const std::string& absPath) {
        std::ifstream in(absPath, std::ios::in | std::ios::binary);
        if (!in.is_open()) {
            return LoadResult<std::string>::Failure("failed to open file: " + absPath);
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return LoadResult<std::string>::Success(ss.str());
    }

    template <size_t N>
    LoadResult<std::array<float, N>> ParseFloatArray(const nlohmann::json& j, const std::string& debugName, const std::string& fieldName) {
        if (!j.is_array() || j.size() != N) {
            return Fail<std::array<float, N>>(debugName + ": '" + fieldName + "' must be float[" + std::to_string(N) + "]");
        }
        std::array<float, N> out{};
        for (size_t i = 0; i < N; ++i) {
            if (!j[i].is_number()) {
                return Fail<std::array<float, N>>(debugName + ": '" + fieldName + "' must contain numeric values");
            }
            out[i] = j[i].get<float>();
        }
        return LoadResult<std::array<float, N>>::Success(std::move(out));
    }

} // namespace jsonutil
