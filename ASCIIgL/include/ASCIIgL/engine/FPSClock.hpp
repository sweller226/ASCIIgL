#include <ASCIIgL/util/Clock.hpp>

#include <deque>

class FPSClock {
public:
    // Singleton accessor
    static FPSClock& GetInst() {
        static FPSClock instance;
        return instance;
    }

    // Delete copy/move
    FPSClock(const FPSClock&) = delete;
    FPSClock& operator=(const FPSClock&) = delete;
    FPSClock(FPSClock&&) = delete;
    FPSClock& operator=(FPSClock&&) = delete;

    void Initialize(unsigned int fpsCap = 60, double fpsWindowSec = 1.0);
    void SetFPSCap(unsigned int fpsCap);
    unsigned int GetFPSCap() const;
    double GetFPS() const;
    void StartFPSClock();
    void EndFPSClock();
    float GetDeltaTime();

private:
    FPSClock() = default;
    ~FPSClock() = default;

    Clock _fpsClock;
    double _fpsWindowSec = 1.0f;
    double _fps = 0.0f;
    double _currDeltaSum = 0.0f;
    std::deque<double> _frameTimes = {};
    unsigned int _fpsCap = 60;

    void CapFPS();
    void FPSSampleCalculate(const double currentDeltaTime);
};