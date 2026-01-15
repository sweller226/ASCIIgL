#include <ASCIIgL/util/Logger.hpp>

#include <fstream>
#include <mutex>
#include <codecvt>
#include <locale>
#include <iomanip>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <vector>

#ifdef _WIN32
    #include <direct.h>
#else
    #include <sys/stat.h>
#endif

namespace {
    std::ofstream logFile;
    std::mutex logMutex;
}

namespace ASCIIgL {

LogLevel Logger::currentLevel = LogLevel::Info;

void Logger::Init(const std::string& filename, LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    currentLevel = level;

    // Create logs folder if it doesn't exist
    size_t pos = filename.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string folder = filename.substr(0, pos);
        #ifdef _WIN32
        _mkdir(folder.c_str());
        #else
        mkdir(folder.c_str(), 0755);
        #endif
    }
    logFile.open(filename, std::ios::out | std::ios::trunc);
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    currentLevel = level;
}

LogLevel Logger::GetLevel() {
    std::lock_guard<std::mutex> lock(logMutex);
    return currentLevel;
}

void Logger::Error(const std::string& message) { LogInternal(LogLevel::Error, message); }
void Logger::Error(const std::wstring& message) { LogInternal(LogLevel::Error, message); }
void Logger::Warning(const std::string& message) { LogInternal(LogLevel::Warning, message); }
void Logger::Warning(const std::wstring& message) { LogInternal(LogLevel::Warning, message); }
void Logger::Info(const std::string& message) { LogInternal(LogLevel::Info, message); }
void Logger::Info(const std::wstring& message) { LogInternal(LogLevel::Info, message); }
void Logger::Debug(const std::string& message) { LogInternal(LogLevel::Debug, message); }
void Logger::Debug(const std::wstring& message) { LogInternal(LogLevel::Debug, message); }

void Logger::LogInternal(LogLevel level, const std::string& message) {
    if (static_cast<int>(level) > static_cast<int>(currentLevel)) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        logFile << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
        
        switch (level) {
            case LogLevel::Error: logFile << "[ERROR] "; break;
            case LogLevel::Warning: logFile << "[WARNING] "; break;
            case LogLevel::Info:  logFile << "[INFO] "; break;
            case LogLevel::Debug: logFile << "[DEBUG] "; break;
        }
        logFile << message << std::endl;
    }
}

void Logger::LogInternal(LogLevel level, const std::wstring& message) {
    if (static_cast<int>(level) > static_cast<int>(currentLevel)) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        logFile << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";

        switch (level) {
            case LogLevel::Error: logFile << "[ERROR] "; break;
            case LogLevel::Warning: logFile << "[WARNING] "; break;
            case LogLevel::Info:  logFile << "[INFO] "; break;
            case LogLevel::Debug: logFile << "[DEBUG] "; break;
        }
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        logFile << conv.to_bytes(message) << std::endl;
    }
}

void Logger::Close() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
}

// ---------- Formatting helpers ----------
static std::string vformat_narrow(const char* fmt, va_list args) {
    // Try a small stack buffer first
    std::vector<char> buf(256);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = std::vsnprintf(buf.data(), buf.size(), fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        // Encoding error; return empty string
        return std::string();
    }
    if (static_cast<size_t>(needed) < buf.size()) {
        return std::string(buf.data(), static_cast<size_t>(needed));
    }

    // Need larger buffer
    buf.resize(static_cast<size_t>(needed) + 1);
    va_list args_copy2;
    va_copy(args_copy2, args);
    int needed2 = std::vsnprintf(buf.data(), buf.size(), fmt, args_copy2);
    va_end(args_copy2);
    if (needed2 < 0) return std::string();
    return std::string(buf.data(), static_cast<size_t>(needed2));
}

static std::wstring vformat_wide(const wchar_t* fmt, va_list args) {
    std::vector<wchar_t> buf(256);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = std::vswprintf(buf.data(), buf.size(), fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) return std::wstring();
    if (static_cast<size_t>(needed) < buf.size()) {
        return std::wstring(buf.data(), static_cast<size_t>(needed));
    }

    buf.resize(static_cast<size_t>(needed) + 1);
    va_list args_copy2;
    va_copy(args_copy2, args);
    int needed2 = std::vswprintf(buf.data(), buf.size(), fmt, args_copy2);
    va_end(args_copy2);
    if (needed2 < 0) return std::wstring();
    return std::wstring(buf.data(), static_cast<size_t>(needed2));
}

void Logger::Errorf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::string s = vformat_narrow(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Error, s);
}
void Logger::Warningf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::string s = vformat_narrow(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Warning, s);
}
void Logger::Infof(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::string s = vformat_narrow(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Info, s);
}
void Logger::Debugf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::string s = vformat_narrow(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Debug, s);
}

// Wide versions
void Logger::Errorf(const wchar_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::wstring ws = vformat_wide(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Error, ws);
}
void Logger::Warningf(const wchar_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::wstring ws = vformat_wide(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Warning, ws);
}
void Logger::Infof(const wchar_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::wstring ws = vformat_wide(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Info, ws);
}
void Logger::Debugf(const wchar_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::wstring ws = vformat_wide(fmt, ap);
    va_end(ap);
    LogInternal(LogLevel::Debug, ws);
}


} // namespace ASCIIgL