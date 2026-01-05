#include <ASCIIgL/util/Logger.hpp>
#include <ASCIICraft/game/Game.hpp>

int main() {
    // Initialize logging
    #ifdef NDEBUG
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Info); // Release build
    #else
        ASCIIgL::Logger::Init("logs/debug.log", ASCIIgL::LogLevel::Debug); // Debug build: log everything
    #endif

    ASCIIgL::Logger::Info("ASCIICraft starting...");

    try {
        // Create and run the game
        Game game;
        game.Run();
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