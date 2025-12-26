#include <ASCIIgL/engine/FPSClock.hpp>

namespace ASCIIgL {

void FPSClock::StartFPSClock() {
    _fpsClock.StartClock();
}

void FPSClock::EndFPSClock() {
    _fpsClock.EndClock();
    FPSSampleCalculate(_fpsClock.GetDeltaTime());
    CapFPS();
}

void FPSClock::CapFPS() {
    const float inverseFrameCap = (1.0f / _fpsCap);

    if (_fpsClock.GetDeltaTime() < inverseFrameCap) {
        std::this_thread::sleep_for(std::chrono::duration<double>(inverseFrameCap - _fpsClock.GetDeltaTime()));
        _fpsClock.SetDeltaTime(inverseFrameCap); // Ensure _deltaTime is at least the frame cap
    }
}

void FPSClock::FPSSampleCalculate(const double currentDeltaTime) {
    _frameTimes.push_back(currentDeltaTime);
    _currDeltaSum += currentDeltaTime;

    while (!_frameTimes.empty() && _currDeltaSum > _fpsWindowSec) {
        const double popped_front = _frameTimes.front();
        _frameTimes.pop_front();
        _currDeltaSum -= popped_front;
    }

    double calculatedFps = _frameTimes.size() * (1 / _currDeltaSum);
    _fps = calculatedFps;
}

float FPSClock::GetDeltaTime() {
	return _fpsClock.GetDeltaTime();
}

void FPSClock::Initialize(unsigned int fpsCap, double fpsWindowSec) {
    _fpsCap = fpsCap;
    _fpsWindowSec = fpsWindowSec;
    _fpsClock.SetDeltaTime(1.0f / static_cast<float>(fpsCap));
}

void FPSClock::SetFPSCap(unsigned int fpsCap) {
    _fpsCap = fpsCap;
}

unsigned int FPSClock::GetFPSCap() const {
    return _fpsCap;
}

double FPSClock::GetFPS() const {
    return _fps;
}

} // namespace ASCIIgL