#include <ASCIICraft/events/EventBus.hpp>

void EventBus::clear() {
    for (auto& [type, buffer] : m_currentFrameEvents) {
        buffer->clear();
    }
}

void EventBus::endFrame() {
    clear();
    // If you add next-frame events later, you'd swap queues here.
}