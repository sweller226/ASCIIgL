#pragma once

#include <string>

namespace ASCIIgL {

enum class LogLevel {
    Error = 0,
    Warning = 1,
    Info = 2,
    Debug = 3,
};

class Logger {
public:
    static void Init(const std::string& filename = "logs/debug.log", LogLevel level = LogLevel::Info);

    static void Error(const std::string& message);
    static void Error(const std::wstring& message);
    static void Warning(const std::string& message);
    static void Warning(const std::wstring& message);
    static void Info(const std::string& message);
    static void Info(const std::wstring& message);
    static void Debug(const std::string& message);
    static void Debug(const std::wstring& message);

    // Formatting helpers (printf-style)
    static void Errorf(const char* fmt, ...);
    static void Warningf(const char* fmt, ...);
    static void Infof(const char* fmt, ...);
    static void Debugf(const char* fmt, ...);

    // Wide-character printf-style
    static void Errorf(const wchar_t* fmt, ...);
    static void Warningf(const wchar_t* fmt, ...);
    static void Infof(const wchar_t* fmt, ...);
    static void Debugf(const wchar_t* fmt, ...);

    static void Close();
    static void SetLevel(LogLevel level);
    static LogLevel GetLevel();
private:
    static LogLevel currentLevel;
    static void LogInternal(LogLevel level, const std::string& message);
    static void LogInternal(LogLevel level, const std::wstring& message);
};

} // namespace ASCIIgL