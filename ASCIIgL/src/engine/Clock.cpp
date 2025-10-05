#include <ASCIIgL/engine/Clock.hpp>

void Clock::StartClock() {
    startTime = std::chrono::system_clock::now();
}

void Clock::EndClock() {
    endTime = std::chrono::system_clock::now();
    std::chrono::duration<float> deltaTimeTemp = endTime - startTime;
    _deltaTime = deltaTimeTemp.count();
}

float Clock::GetDeltaTime() const {
    return _deltaTime;
}

void Clock::SetDeltaTime(float dt) {
    _deltaTime = dt;
}