#pragma once

#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <memory>
#include <utility>
#include <cassert>

class EventBus {
public:
    // Emit an event (universal reference)
    template<typename T>
    void emit(T&& e) {
        using U = std::decay_t<T>; // strip &, const, volatile
        auto& vec = getOrCreateBuffer<U>();
        vec.emplace_back(std::forward<T>(e));
    }

    // Access all events of type T emitted this frame
    template<typename T>
    std::vector<T>& view() {
        auto it = m_currentFrameEvents.find(std::type_index(typeid(T)));
        if (it == m_currentFrameEvents.end()) {
            static std::vector<T> empty;
            return empty;
        }
        return static_cast<Buffer<T>*>(it->second.get())->data;
    }

    // Const view
    template<typename T>
    const std::vector<T>& view() const {
        auto it = m_currentFrameEvents.find(std::type_index(typeid(T)));
        if (it == m_currentFrameEvents.end()) {
            static std::vector<T> empty;
            return empty;
        }
        return static_cast<const Buffer<T>*>(it->second.get())->data;
    }

    void clear();

    void endFrame();

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
