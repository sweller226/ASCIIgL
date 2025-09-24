#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIICraft/game/Game.hpp>

int main() {
    // Initialize logging
    #ifdef NDEBUG
        Logger::Init("logs/debug.log", LogLevel::Info); // Release build
    #else
        Logger::Init("logs/debug.log", LogLevel::Debug); // Debug build: log everything
    #endif

    Logger::Info("ASCIICraft starting...");

    try {
        // Create and run the game
        Game game;
        game.Run();
        game.Shutdown();
    }
    catch (const std::exception& e) {
        Logger::Error("Game crashed with exception: " + std::string(e.what()));
    }
    catch (...) {
        Logger::Error("Game crashed with unknown exception");
    }

    Logger::Info("ASCIICraft exited");
    Logger::Close();
    
    return 0;
}