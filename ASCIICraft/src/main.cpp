#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIICraft/game/Game.hpp>

#include <string>

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

        // Exit when user closes window or console (handled by ASCIIgL::Screen)
        game.Run([]() { return ASCIIgL::Screen::GetInst().ShouldExit(); }, renderToTerminal);

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