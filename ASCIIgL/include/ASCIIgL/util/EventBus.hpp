#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ASCIIgL {

class EventBus {
public:
    template<typename T>
    void emit(T&& event) {
        using U = std::decay_t<T>;
        auto& vec = getOrCreateBuffer<U>();
        vec.emplace_back(std::forward<T>(event));
    }

    template<typename T>
    std::vector<T>& view() {
        auto it = m_currentFrameEvents.find(std::type_index(typeid(T)));
        if (it == m_currentFrameEvents.end()) {
            static std::vector<T> empty;
            return empty;
        }
        return static_cast<Buffer<T>*>(it->second.get())->data;
    }

    template<typename T>
    const std::vector<T>& view() const {
        auto it = m_currentFrameEvents.find(std::type_index(typeid(T)));
        if (it == m_currentFrameEvents.end()) {
            static std::vector<T> empty;
            return empty;
        }
        return static_cast<const Buffer<T>*>(it->second.get())->data;
    }

    void clear() {
        for (auto& [type, buffer] : m_currentFrameEvents) {
            (void)type;
            buffer->clear();
        }
    }

    void endFrame() {
        clear();
    }

private:
    struct IBuffer {
        virtual ~IBuffer() = default;
        virtual void clear() = 0;
    };

    template<typename T>
    struct Buffer : IBuffer {
        std::vector<T> data;
        void clear() override { data.clear(); }
    };

    using BufferPtr = std::unique_ptr<IBuffer>;
    using BufferMap = std::unordered_map<std::type_index, BufferPtr>;

    BufferMap m_currentFrameEvents;

    template<typename T>
    std::vector<T>& getOrCreateBuffer() {
        auto key = std::type_index(typeid(T));
        auto it = m_currentFrameEvents.find(key);

        if (it == m_currentFrameEvents.end()) {
            auto buf = std::make_unique<Buffer<T>>();
            auto* raw = buf.get();
            m_currentFrameEvents.emplace(key, std::move(buf));
            return raw->data;
        }

        return static_cast<Buffer<T>*>(it->second.get())->data;
    }
};

} // namespace ASCIIgL
