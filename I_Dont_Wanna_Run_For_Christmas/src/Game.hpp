#pragma once

#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/engine/GameObj.hpp>

// Game Code Includes
#include "Player.hpp"
#include "Enemies.hpp"
#include "Present.hpp"

// Built In Libraries
#include <map>
#include <Windows.h>
#include "MMSystem.h"

class Game // This class represents the game itself, it is a singleton as there only needs to be one "game"
{
private:
	enum STATE // these are all states that the game can be in
	{
		MAIN_MENU,
		HOW_TO_PLAY,
		GAME_LORE,
		MAZE,
		CAUGHT,
		WIN,
	};

	unsigned int BtnSelected = 0; // just a number that can be either 0 or 1 for if its selecting the first or second button on the main menu

	// singleton class setup
	Game();
	static inline Game* Instance = nullptr;

	// game starts at the main menu
	unsigned int gameState = MAIN_MENU;

	// Screen dimensions compatible with both Windows Terminal and Command Prompt
	// Command Prompt max: 960x504, so we use safe values below that
	unsigned int SCR_WIDTH = 900;   // Was 1350, reduced for CMD compatibility
	unsigned int SCR_HEIGHT = 600;  // Was 900, reduced for CMD compatibility

	bool running = true;
	VERTEX_SHADER vertexShader;  // default vertex shader, didn't have time to implement custom ones

	// LOADING LEVEL
	int levelXSize = 300;
	int levelZSize = 300;
	int levelHeight = 300;

	void LoadLevel();

	// MENU TEXTURES
	std::map<std::string, Texture*> Textures; // textures are loaded in on the heap and stored through a map with a string tag attached to them

	// Game Objects
	GameObj* Level = nullptr;
	std::vector<Enemy*> enemies;
	std::vector<Present*> presents;

	// camera used when drawing gui elements
	Camera2D guiCamera;

	// all models used in the game
	Model* LevelModel;
	Model* MariahModel;
	Model* PresentModel;
	Model* Mariah2Model;

	// different game loops for different game states
	void RunMainMenu();
	void RunHowToPlay();
	void RunLore();
	void RunMaze();
	void RunLost();
	void RunWin();

	// this function is different from load level, as that loads everyting into the game code, but this puts it into the level
	void initLevel();

	// function that controls all of the enemies movements
	void MariahAI();

	// just gets the number of presents, I don't store this as a variable because it was simpler to just iterate over every present
	// and check if it was collected and find out how many were collected that way instead of keeping track with a variable
	int GetPresentsCollected();

public:
	// some more singleton stuff, as singletons can't be copied we need to delete the copy constructor
	Game(const Game& obj) = delete;
	~Game();

	// general main loop of the game
	void Run();

	// singleton method that returns an instance of the game, if there isnt one already it creates one
	static Game* GetInstance();

	// I have so many things on heap memory at this point I must be leaking memory lord help me
	Player* player = nullptr;

};