#include <ASCIIgL/util/Profiler.hpp>
#include <ASCIIgL/engine/Logger.hpp>
#include <iostream>
#include <iomanip>

void Profiler::BeginFrame()
{
    if (!_enabled) return;

    _inFrame = true;
    _frameStartTime = Clock::now();
    _currentFrameData.clear();
    _activeTimings.clear();
}

void Profiler::EndFrame()
{
    if (!_enabled || !_inFrame) return;

    // Calculate total frame time
    auto frameEndTime = Clock::now();
    _totalFrameTime_ms = Duration(frameEndTime - _frameStartTime).count();
    
    // Accumulate current frame data into historical data
    for (const auto& [name, entry] : _currentFrameData) {
        auto& historical = _historicalData[name];
        
        // Add to samples
        historical.samples.push_back(entry.duration_ms);
        historical.totalTime_ms += entry.duration_ms;
        historical.callCount++;
        
        // Keep only the last N frames
        if (historical.samples.size() > _maxFramesToAverage) {
            double oldestSample = historical.samples.front();
            historical.totalTime_ms -= oldestSample;
            historical.samples.erase(historical.samples.begin());
        }
    }
    
    _frameCount++;
    _inFrame = false;
}

void Profiler::BeginSection(const std::string& name)
{
    if (!_enabled || !_inFrame) return;

    TimePoint startTime = Clock::now();
    _activeTimings.push_back({name, startTime});
}

void Profiler::EndSection(const std::string& name)
{
    if (!_enabled || !_inFrame) return;

    TimePoint endTime = Clock::now();
    
    // Find the matching begin section (allows nested profiling)
    for (auto it = _activeTimings.rbegin(); it != _activeTimings.rend(); ++it) {
        if (it->first == name) {
            double duration_ms = Duration(endTime - it->second).count();
            
            // Accumulate time if section was already recorded this frame
            if (_currentFrameData.find(name) != _currentFrameData.end()) {
                _currentFrameData[name].duration_ms += duration_ms;
            } else {
                _currentFrameData[name] = {it->second, duration_ms};
            }
            
            // Remove from active stack
            _activeTimings.erase(std::next(it).base());
            return;
        }
    }
    
    // If we get here, there was a mismatch (EndSection without BeginSection)
    Logger::Warning("Profiler: EndSection(\"" + name + "\") called without matching BeginSection");
}

std::vector<Profiler::SectionStats> Profiler::GetStats() const
{
    std::vector<SectionStats> stats;
    
    if (_historicalData.empty() || _totalFrameTime_ms <= 0.0) {
        return stats;
    }
    
    // Calculate average frame time across samples
    double avgTotalFrameTime = 0.0;
    for (const auto& [name, data] : _historicalData) {
        avgTotalFrameTime = std::max(avgTotalFrameTime, data.totalTime_ms);
    }
    
    // Use actual total frame time if we have it
    if (_totalFrameTime_ms > 0.0) {
        avgTotalFrameTime = _totalFrameTime_ms;
    }
    
    // Build statistics for each section
    for (const auto& [name, data] : _historicalData) {
        if (data.samples.empty()) continue;
        
        SectionStats section;
        section.name = name;
        section.totalTime_ms = data.totalTime_ms;
        section.avgTime_ms = data.totalTime_ms / static_cast<double>(data.samples.size());
        section.callCount = data.callCount;
        
        // Calculate min and max
        section.minTime_ms = *std::min_element(data.samples.begin(), data.samples.end());
        section.maxTime_ms = *std::max_element(data.samples.begin(), data.samples.end());
        
        // Calculate percentage of frame time
        // Use total time divided by actual frame count, not sample count
        // This gives accurate percentage even for sections called infrequently
        double avgTimePerFrame = data.totalTime_ms / static_cast<double>(_maxFramesToAverage);
        section.percentage = (avgTimePerFrame / avgTotalFrameTime) * 100.0;
        
        stats.push_back(section);
    }
    
    // Sort by percentage (highest first)
    std::sort(stats.begin(), stats.end(), 
        [](const SectionStats& a, const SectionStats& b) {
            return a.percentage > b.percentage;
        });
    
    return stats;
}

std::string Profiler::GetReport(bool sortByPercentage) const
{
    std::vector<SectionStats> stats = GetStats();
    
    if (stats.empty()) {
        return "Profiler: No data collected yet. Make sure to call BeginFrame() and EndFrame().\n";
    }
    
    if (!sortByPercentage) {
        std::sort(stats.begin(), stats.end(),
            [](const SectionStats& a, const SectionStats& b) {
                return a.name < b.name;
            });
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    // Header
    oss << "\n==================== PROFILER REPORT ====================\n";
    oss << "Frames Averaged: " << _frameCount << " (max: " << _maxFramesToAverage << ")\n";
    oss << "Total Frame Time: " << _totalFrameTime_ms << " ms\n";
    oss << "-------------------------------------------------------------\n";
    oss << std::left << std::setw(40) << "Section"
        << std::right << std::setw(10) << "Avg (ms)"
        << std::setw(10) << "Min (ms)"
        << std::setw(10) << "Max (ms)"
        << std::setw(10) << "% Frame"
        << std::setw(8) << "Calls"
        << "\n";
    oss << "-------------------------------------------------------------\n";
    
    // Data rows
    for (const auto& section : stats) {
        oss << std::left << std::setw(40) << section.name
            << std::right << std::setw(10) << section.avgTime_ms
            << std::setw(10) << section.minTime_ms
            << std::setw(10) << section.maxTime_ms
            << std::setw(9) << section.percentage << "%"
            << std::setw(8) << section.callCount
            << "\n";
    }
    
    oss << "=============================================================\n";
    
    return oss.str();
}

void Profiler::LogReport(bool sortByPercentage) const
{
    std::string report = GetReport(sortByPercentage);
    Logger::Info(report);
}

void Profiler::Reset()
{
    _historicalData.clear();
    _currentFrameData.clear();
    _activeTimings.clear();
    _frameCount = 0;
    _totalFrameTime_ms = 0.0;
    _inFrame = false;
}
