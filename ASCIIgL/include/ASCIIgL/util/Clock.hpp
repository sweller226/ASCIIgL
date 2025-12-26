#pragma once

#include <chrono>
#include <thread>

namespace ASCIIgL {

class Clock {
private:
    std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();

    float _deltaTime = 0.0f;

public:
    Clock() = default;

    void StartClock();
    void EndClock();
    float GetDeltaTime() const;
    void SetDeltaTime(float dt);
};

} // namespace ASCIIgL
