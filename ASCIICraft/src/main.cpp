#include <windows.h>
#include <atomic>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIICraft/game/Game.hpp>

// Global exit flag
static std::atomic<bool> g_shouldExit = false;

// Windows console control handler
BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_shouldExit = true;

            return TRUE;
    }
    return FALSE;
}

int main() {
    // Initialize logging
    #ifdef NDEBUG
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Info);
    #else
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Debug);
    #endif

    ASCIIgL::Logger::Info("ASCIICraft starting...");

    // Register Windows Terminal–compatible control handler
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    try {
        Game game;

        // Run until user exits or window closes
        game.Run([&]() { return g_shouldExit.load(); });

        game.Shutdown();
    }
    catch (const std::exception& e) {
        ASCIIgL::Logger::Error("Game crashed with exception: " + std::string(e.what()));
    }
    catch (...) {
        ASCIIgL::Logger::Error("Game crashed with unknown exception");
    }

    ASCIIgL::Logger::Info("ASCIICraft exited");
    ASCIIgL::Logger::Close();

    return 0;
}