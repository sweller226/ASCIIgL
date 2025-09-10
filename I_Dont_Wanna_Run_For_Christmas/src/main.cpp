#include "Game.hpp"
#include <ASCIIgL/engine/Logger.hpp>

int main()
{
	//std::vector<VERTEX> vertices {
	//	VERTEX({ -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // Bottom-left
	//	VERTEX({  1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	//	VERTEX({  1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({  1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	//	VERTEX({ -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
	//	VERTEX({ -1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//												 
	//	VERTEX({ -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
	//	VERTEX({  1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	//	VERTEX({  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	//	VERTEX({ -1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//	VERTEX({ -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
	//		 										 
	//	VERTEX({ -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	//	VERTEX({ -1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//	VERTEX({ -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
	//	VERTEX({ -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
	//	VERTEX({ -1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({ -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	// 												 
	//	VERTEX({  1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//	VERTEX({  1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({  1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right         
	//	VERTEX({  1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({  1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//	VERTEX({  1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left     
	//												 
	//	VERTEX({ -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	//	VERTEX({  1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//	VERTEX({  1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
	//	VERTEX({  1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
	//	VERTEX({ -1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({ -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
	//												 
	//	VERTEX({ -1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//	VERTEX({  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({  1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right     
	//	VERTEX({  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
	//	VERTEX({ -1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
	//	VERTEX({ -1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left     
	//};
	
	#ifdef NDEBUG
		Logger::Init("logs/debug.log", LogLevel::Error); // Release build: only log errors
	#else
		Logger::Init("logs/debug.log", LogLevel::Debug); // Debug build: log everything
	#endif

	Logger::Info("Game started!");

	Game::GetInstance()->Run();

	Logger::Close();
}