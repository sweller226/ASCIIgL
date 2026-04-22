#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIICraft/game/Game.hpp>

#include <string>

#include <windows.h>

namespace {
    Game* g_activeGame = nullptr;

    BOOL WINAPI ConsoleCloseHandler(DWORD ctrlType) {
        switch (ctrlType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_SHUTDOWN_EVENT:
            case CTRL_LOGOFF_EVENT:
                if (g_activeGame) {
                    g_activeGame->Shutdown();
                }
                return TRUE;
            default:
                return FALSE;
        }
    }

    class ConsoleHandlerScope {
    public:
        explicit ConsoleHandlerScope(Game* game)
            : game_(game)
            , handlerInstalled_(SetConsoleCtrlHandler(ConsoleCloseHandler, TRUE) == TRUE) {
            g_activeGame = game_;
        }

        ~ConsoleHandlerScope() {
            g_activeGame = nullptr;
            if (handlerInstalled_) {
                SetConsoleCtrlHandler(ConsoleCloseHandler, FALSE);
            }
        }

        ConsoleHandlerScope(const ConsoleHandlerScope&) = delete;
        ConsoleHandlerScope& operator=(const ConsoleHandlerScope&) = delete;

    private:
        Game* game_;
        bool handlerInstalled_;
    };
}

static bool ParseRenderToTerminal(int argc, char* argv[]) {
    bool renderToTerminal = true; // default: terminal mode
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--terminal" || arg == "-t")
            renderToTerminal = true;
        else if (arg == "--window" || arg == "-w")
            renderToTerminal = false;
    }
    return renderToTerminal;
}

int main(int argc, char* argv[]) {
    // Initialize logging
    #ifdef NDEBUG
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Info);
    #else
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Debug);
    #endif

    ASCIIgL::Logger::Info("ASCIICraft starting...");

    bool renderToTerminal = ParseRenderToTerminal(argc, argv);

    try {
        Game game;
        ConsoleHandlerScope closeHandler(&game);

        // Exit when user closes window or console (handled by ASCIIgL::Screen)
        game.Run([]() { return ASCIIgL::Screen::GetInst().ShouldExit(); }, renderToTerminal);
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