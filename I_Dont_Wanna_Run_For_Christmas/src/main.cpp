#include "Game.hpp"
#include <ASCIIgL/engine/Logger.hpp>

int main()
{
	#ifdef NDEBUG
		Logger::Init("logs/debug.log", LogLevel::Error); // Release build: only log errors
	#else
		Logger::Init("logs/debug.log", LogLevel::Debug); // Debug build: log everything
	#endif

	Logger::Info("Game started!");

	Game::GetInstance()->Run();

	Logger::Close();
}