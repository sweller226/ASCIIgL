#include "Game.hpp"
#include <ASCIIgL/engine/Logger.hpp>

int main()
{
	#ifdef NDEBUG
		Logger::Init("logs/debug.log", LogLevel::Info); // Release build: only log info, warnings, and errors
	#else
		Logger::Init("logs/debug.log", LogLevel::Debug); // Debug build: log everything
	#endif

	Logger::Info("Game started!");

	Game::GetInst()->Run();

	Logger::Close();
}