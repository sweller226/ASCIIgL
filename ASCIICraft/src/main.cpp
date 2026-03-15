#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIICraft/game/Game.hpp>

int main() {
    // Initialize logging
    #ifdef NDEBUG
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Info);
    #else
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Debug);
    #endif

    ASCIIgL::Logger::Info("ASCIICraft starting...");

    try {
        Game game;

        // Exit when user closes window or console (handled by ASCIIgL::Screen)
        game.Run([]() { return ASCIIgL::Screen::GetInst().ShouldExit(); });

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