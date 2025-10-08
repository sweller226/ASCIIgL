#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

class Profiler
{
public:
    struct SectionStats {
        std::string name;
        double totalTime_ms = 0.0;
        double avgTime_ms = 0.0;
        double minTime_ms = 1e9;
        double maxTime_ms = 0.0;
        double percentage = 0.0;
        unsigned int callCount = 0;
    };

private:
    Profiler() = default;
    ~Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    // High-resolution clock
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double, std::milli>;

    // Current frame timing data
    struct TimingEntry {
        TimePoint startTime;
        double duration_ms = 0.0;
    };

    // Historical data for averaging
    struct HistoricalData {
        std::vector<double> samples;
        double totalTime_ms = 0.0;
        unsigned int callCount = 0;
    };

    // Active timing stack for nested profiling
    std::vector<std::pair<std::string, TimePoint>> _activeTimings;
    
    // Current frame data
    std::unordered_map<std::string, TimingEntry> _currentFrameData;
    
    // Historical data for averaging across frames
    std::unordered_map<std::string, HistoricalData> _historicalData;
    
    // Frame tracking
    TimePoint _frameStartTime;
    double _totalFrameTime_ms = 0.0;
    unsigned int _frameCount = 0;
    unsigned int _maxFramesToAverage = 60; // Default: average over 60 frames
    
    // Profiler state
    bool _enabled = true;
    bool _inFrame = false;

public:
    static Profiler& GetInst() {
        static Profiler instance;
        return instance;
    }

    void SetEnabled(bool enabled) { _enabled = enabled; }
    bool IsEnabled() const { return _enabled; }

    void SetAveragingFrames(unsigned int frames) { _maxFramesToAverage = frames; }
    unsigned int GetAveragingFrames() const { return _maxFramesToAverage; }

    void BeginFrame();
    void EndFrame();

    void BeginSection(const std::string& name);
    void EndSection(const std::string& name);

    std::vector<SectionStats> GetStats() const;
    std::string GetReport(bool sortByPercentage = true) const;
    void LogReport(bool sortByPercentage = true) const;
    void Reset();
    double GetTotalFrameTime() const { return _totalFrameTime_ms; }
    unsigned int GetFrameCount() const { return _frameCount; }
};

/**
 * @brief RAII helper class for automatic profiling scope management
 */
class ProfileScope
{
public:
    explicit ProfileScope(const std::string& name) : _name(name) {
        Profiler::GetInst().BeginSection(_name);
    }
    
    ~ProfileScope() {
        Profiler::GetInst().EndSection(_name);
    }

private:
    std::string _name;
};

// Convenience macro for profiling a scope
#define PROFILE_SCOPE(name) ProfileScope _profileScope##__LINE__(name)

// Convenience macro that can be easily disabled in release builds
#ifdef NDEBUG
    #define PROFILE_SCOPE_DEBUG(name) ((void)0)
#else
    #define PROFILE_SCOPE_DEBUG(name) PROFILE_SCOPE(name)
#endif
