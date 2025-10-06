#include "Game.hpp"

#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIIgL/engine/Collision.hpp>

Game::Game()
	: guiCamera(glm::vec2(0, 0), SCR_WIDTH, SCR_HEIGHT)
{
	// don't mind me just loading in 100030423 textures for my simple gui
	Textures["Title"] = new Texture("res/textures/GUI/Title.png");
	Textures["Start_Sel"] = new Texture("res/textures/GUI/StartSelected.png");
	Textures["Start_Unsel"] = new Texture("res/textures/GUI/StartUnselected.png");
	Textures["How_To_Play_Sel"] = new Texture("res/textures/GUI/HowToPlaySelected.png");
	Textures["How_To_Play_Unsel"] = new Texture("res/textures/GUI/HowToPlayUnselected.png");
	Textures["GameInfo1"] = new Texture("res/textures/GUI/GameInfo1.png");
	Textures["GameInfo2"] = new Texture("res/textures/GUI/GameInfo2.png");
	Textures["Select_Btn"] = new Texture("res/textures/GUI/PressQ.png");
	Textures["BackInfo"] = new Texture("res/textures/GUI/BackInfo.png");
	Textures["Lost"] = new Texture("res/textures/GUI/Lost.png");
	Textures["Win"] = new Texture("res/textures/GUI/Win.png");

	// stamina bar gui textures
	Textures["Tired"] = new Texture("res/textures/GUI/Tired.png");
	Textures["Stamina1"] = new Texture("res/textures/GUI/Stamina1.png");
	Textures["Stamina2"] = new Texture("res/textures/GUI/Stamina2.png");
	Textures["Stamina3"] = new Texture("res/textures/GUI/Stamina3.png");
	Textures["Stamina4"] = new Texture("res/textures/GUI/Stamina4.png");
	Textures["Stamina5"] = new Texture("res/textures/GUI/Stamina5.png");
	Textures["Stamina6"] = new Texture("res/textures/GUI/Stamina6.png");
}

Game::~Game() {
	// this destructor just deletes everything
	for (auto const& [key, val] : Textures)
	{
		delete val;
	}

	for (auto i : enemies)
	{
		delete i;
	}

	delete Instance, player, LevelModel, Level, PresentModel, MariahModel, Mariah2Model;
}

Game* Game::GetInst() {
	if (Instance == nullptr)
	{
		Instance = new Game();
		return Instance;
	}
	else
	{
		return Instance;
	}
}

void Game::Run() {
    Logger::Info("Game loop starting.");

    Logger::Info("Setting up palette and screen...");
	std::array<PaletteEntry, 15> paletteEntries = {{
		{ {1, 1, 1}, 0x1 },        // Very Dark Gray
		{ {2, 2, 2}, 0x2 },        // Darker Gray
		{ {3, 3, 3}, 0x3 },        // Dark Gray
		{ {0, 0, 12}, 0x4 },       // Blue (was Charcoal Gray)
		{ {0, 12, 0}, 0x5 },       // Green
		{ {12, 0, 0}, 0x6 },       // Red
		{ {14, 12, 10}, 0x7 },     // Skin Tone 1 (light)
		{ {12, 9, 6}, 0x8 },       // Skin Tone 2 (medium)
		{ {9, 9, 9}, 0x9 },        // Light Neutral Gray
		{ {10, 10, 10}, 0xA },     // Light Gray
		{ {11, 11, 11}, 0xB },     // Brighter Gray
		{ {12, 12, 12}, 0xC },     // Pale Gray
		{ {13, 13, 13}, 0xD },     // Very Pale Gray
		{ {14, 14, 14}, 0xE },     // Near White
		{ {15, 15, 15}, 0xF }      // White
	}};
    Palette gamePalette = Palette(paletteEntries); // Default palette

	Screen::GetInst().Initialize(SCR_WIDTH, SCR_HEIGHT, L"I Don't Wanna Run For Christmas", 3, 60, 1.0f, gamePalette);
	Renderer::GetInst().Initialize();

	Logger::Info("Loading level...");
	SCR_WIDTH = Screen::GetInst().GetWidth();
	SCR_HEIGHT = Screen::GetInst().GetHeight();
	guiCamera.setScreenDimensions(SCR_WIDTH, SCR_HEIGHT);
    Renderer::GetInst().SetWireframe(false);
    Renderer::GetInst().SetBackfaceCulling(true);
    Renderer::GetInst().SetCCW(true);
	Renderer::GetInst().SetAntialiasingsamples(8);
	Renderer::GetInst().SetAntialiasing(true);

    // Logger::Info("Playing background music.");
	// BOOL soundResult = PlaySound(TEXT(".\\res\\audio\\Man.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
	// if (soundResult)
	// 	Logger::Info("Background music started successfully.");
	// else
	// 	Logger::Error("Failed to start background music.");

    // main gameloop
    while (running == true)
    {
        Screen::GetInst().StartFPSClock();
        Screen::GetInst().ClearPixelBuffer();
		Renderer::GetInst().ClearBuffers();

        // // do game logic here
        switch (gameState) {
            case MAIN_MENU:
                RunMainMenu();
                break;
            case HOW_TO_PLAY:
                RunHowToPlay();
                break;
            case GAME_LORE:
                RunLore();
                break;
            case MAZE:
                RunMaze();
                break;
            case CAUGHT:
                RunLost();
                break;
            case WIN:
                RunWin();
                break;
            default:
                Logger::Warning("[WARN] Unknown game state: " + std::to_string(gameState));
                break;
        }

		Renderer::GetInst().OverwritePxBuffWithColBuff();
        Renderer::GetInst().DrawScreenBorderPxBuff(0xF);
        Screen::GetInst().OutputBuffer();

		Screen::GetInst().EndFPSClock();
        Screen::GetInst().RenderTitle(true);
    }
    Logger::Info("Game loop ended.");
}

void Game::LoadLevel() {
	LevelModel = new Model("res/models/level2/MazeTest.obj");
	MariahModel = new Model("res/models/mariah/mariah.obj");
	Mariah2Model = new Model("res/models/Mariah2/mariah.obj");
	PresentModel = new Model("res/models/Present/present.obj");

	initLevel();
}

void Game::RunMainMenu() {
	Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Title"], glm::vec2(0.489f, 0.267f), 0.0f, glm::vec2(0.289f, 0.167f), guiCamera, 0);
	Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Select_Btn"], glm::vec2(0.233f, 0.9f), 0.0f, glm::vec2(0.222f, 0.05f), guiCamera, 0);

	if (GetKeyState(VK_UP) & 0x8000)
	{
		BtnSelected = 0;
	}
	if (GetKeyState(VK_DOWN) & 0x8000)
	{
		BtnSelected += 1;
	}

	if (GetKeyState('Q') & 0x8000)
	{
		if (BtnSelected != 0)
		{
			gameState = GAME_LORE;
			Sleep(100);
		}
		else
		{
			gameState = HOW_TO_PLAY;
			Sleep(100);
		}
	}

	if (BtnSelected != 0)
	{
		// How to Play Selected: position (215, 150) -> (215/450, 150/300) = (0.478, 0.5), size (75, 30) -> (75/450, 30/300) = (0.167, 0.1)
		Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["How_To_Play_Sel"], glm::vec2(0.478f, 0.5f), 0.0f, glm::vec2(0.167f, 0.1f), guiCamera, 0);
		// Start Unselected: position (217, 200) -> (217/450, 200/300) = (0.482, 0.667), size (60, 30) -> (60/450, 30/300) = (0.133, 0.1)
		Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Start_Unsel"], glm::vec2(0.482f, 0.667f), 0.0f, glm::vec2(0.133f, 0.1f), guiCamera, 0);
	}
	else
	{
		// How to Play Unselected: position (215, 150) -> (215/450, 150/300) = (0.478, 0.5), size (75, 30) -> (75/450, 30/300) = (0.167, 0.1)
		Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["How_To_Play_Unsel"], glm::vec2(0.478f, 0.5f), 0.0f, glm::vec2(0.167, 0.1), guiCamera, 0);
		// Start Selected: position (217, 200) -> (217/450, 200/300) = (0.482, 0.667), size (60, 30) -> (60/450, 30/300) = (0.133, 0.1)
		Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Start_Sel"], glm::vec2(0.482f, 0.667f), 0.0f, glm::vec2(0.133f, 0.1f), guiCamera, 0);
	}
	
}

void Game::RunHowToPlay() {
	if (GetKeyState('Q') & 0x8000)
	{
		gameState = MAIN_MENU;
		Sleep(100);
		BtnSelected = 0;
		
	}

	// Game Info 2: position (215, 120) -> (215/450, 120/300) = (0.478, 0.4), size (175, 100) -> (175/450, 100/300) = (0.389, 0.333)
	Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["GameInfo2"], glm::vec2(0.478f, 0.4f), 0.0f, glm::vec2(0.389f, 0.333f), guiCamera, 0);
	// Back Info: position (217, 250) -> (217/450, 250/300) = (0.482, 0.833), size (100, 15) -> (100/450, 15/300) = (0.222, 0.05)
	Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["BackInfo"], glm::vec2(0.482f, 0.833f), 0.0f, glm::vec2(0.222f, 0.05f), guiCamera, 0);

}

void Game::RunLore() {
	// Game Info 1: position (225, 150) -> (225/450, 150/300) = (0.5, 0.5), size (200, 125) -> (200/450, 125/300) = (0.444, 0.417)
	Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["GameInfo1"], glm::vec2(0.5f, 0.5f), 0.0f, glm::vec2(0.444f, 0.417f), guiCamera, 0);
	Renderer::GetInst().OverwritePxBuffWithColBuff();
	Screen::GetInst().OutputBuffer();
	Sleep(7500); // this is me being super lazy, instead of implementing a timer that lets the main loop run I just pause the whole thing for 7.5 seconds :p
	Screen::GetInst().StartFPSClock();
	gameState = MAZE;
	LoadLevel();
}

void Game::RunMaze() {
	player->Update(Level);
	MariahAI();

	for (int i = 0; i < enemies.size(); i++)
	{
		glm::mat4 model = glm::inverse(glm::lookAt(glm::vec3(enemies[i]->position.x, enemies[i]->position.y, enemies[i]->position.z), 
			glm::vec3(player->GetPlayerPos().x, enemies[i]->position.y, player->GetPlayerPos().z), glm::vec3(0, 1, 0)));
		model = glm::scale(model, glm::vec3(-enemies[i]->size.x, enemies[i]->size.y, enemies[i]->size.z));

		Renderer::GetInst().DrawModel(vertexShader, *enemies[i]->model, model, player->GetCamera());

		bool hit = Collision::DoesPointCircleCol(glm::vec2(enemies[i]->position.x, enemies[i]->position.z), glm::vec2(player->GetPlayerPos().x, player->GetPlayerPos().z), player->GetPlayerHitBoxRad());
		if (hit == true)
		{
			gameState = CAUGHT;
		}
	}

	for (int i = 0; i < presents.size(); i++)
	{
		if (presents[i]->collected != true)
		{
			glm::mat4 model = glm::inverse(glm::lookAt(glm::vec3(presents[i]->position.x, presents[i]->position.y, presents[i]->position.z), 
				glm::vec3(player->GetPlayerPos().x, presents[i]->position.y, player->GetPlayerPos().z), glm::vec3(0, 1, 0)));
			model = glm::scale(model, glm::vec3(-presents[i]->size.x, presents[i]->size.y, presents[i]->size.z));

			Renderer::GetInst().DrawModel(vertexShader, *presents[i]->model, model, player->GetCamera());

			bool hit = Collision::DoesPointCircleCol(glm::vec2(presents[i]->position.x, presents[i]->position.z), glm::vec2(player->GetPlayerPos().x, player->GetPlayerPos().z), player->GetPlayerHitBoxRad());
			if (hit == true)
			{
				presents[i]->collected = true;

				enemies.push_back(new Enemy(glm::vec3(0, 20, 0), glm::vec3(10, 10, 0), Mariah2Model));
			}
		}
	}
	
	// Stamina bar: position (380, 270) -> (380/450, 270/300) = (0.844, 0.9), size (50, 20) -> (50/450, 20/300) = (0.111, 0.067)
	glm::vec2 barSize = glm::vec2(0.111f, 0.067f);
	glm::vec2 barPos = glm::vec2(0.844f, 0.9f);

	unsigned int chunk = player->GetStaminaChunk(6, 0.05);
	switch (chunk) {
		case 6:
			Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Stamina6"], barPos, 0.0f, barSize, guiCamera, 0);
			break;
		case 5:
			Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Stamina5"], barPos, 0.0f, barSize, guiCamera, 0);
			break;
		case 4:
			Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Stamina4"], barPos, 0.0f, barSize, guiCamera, 0);
			break;
		case 3:
			Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Stamina3"], barPos, 0.0f, barSize, guiCamera, 0);
			break;
		case 2:
			Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Stamina2"], barPos, 0.0f, barSize, guiCamera, 0);
			break;
		case 1:
			Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Stamina1"], barPos, 0.0f, barSize, guiCamera, 0);
			break;
		default:
			Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Tired"], barPos, 0.0f, barSize, guiCamera, 0);
			break;
	}

	int presentsCollected = GetPresentsCollected();
	if (presentsCollected == presents.size() && presentsCollected != 0) {
		gameState = WIN;
	}

	Renderer::GetInst().DrawModel(vertexShader, *Level->model, Level->position, Level->rotation, Level->size, player->GetCamera());
}

void Game::RunLost() {
	Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Lost"], glm::vec2(0.5, 0.5), 0.0f, glm::vec2(0.444, 0.417), guiCamera, 0);

	if (GetKeyState('R') & 0x8000)
	{
		gameState = MAZE;
		
		delete player, Level;

		for (int i = 0; i < enemies.size(); i++)
		{
			delete enemies[i];
		}

		for (int i = 0; i < presents.size(); i++)
		{
			delete presents[i];
		}

		presents.clear();
		enemies.clear();
		
		initLevel();

		Sleep(100);
	}
}

void Game::MariahAI() {
	float chaseSpeed = 40.0f;
	float patrolSpeed = 130.0f;

	for (int i = 0; i < enemies.size(); i++)
	{
		if (enemies[i]->aiState == Enemy::CHASE)
		{
			glm::vec3 move = glm::normalize(player->GetPlayerPos() - enemies[i]->position) * Screen::GetInst().GetDeltaTime() * chaseSpeed;
			enemies[i]->position += glm::vec3(move.x, 0, move.z);
		}

		if (enemies[i]->aiState == Enemy::PATROL)
		{
			glm::vec2 pos1 = glm::vec2(enemies[i]->position.x, enemies[i]->position.z);
			glm::vec2 pos2 = glm::vec2(enemies[i]->patrolDest.x, enemies[i]->patrolDest.z);

			if (Collision::DoesPointCircleCol(pos1, pos2, enemies[i]->destRadius))
			{
				if (enemies[i]->patrolDest == enemies[i]->patrolEnd)
					enemies[i]->patrolDest = enemies[i]->patrolStart;
				else
					enemies[i]->patrolDest = enemies[i]->patrolEnd;
			}
			glm::vec3 move = glm::normalize(enemies[i]->patrolDest - enemies[i]->position) * Screen::GetInst().GetDeltaTime() * patrolSpeed;
			enemies[i]->position += glm::vec3(move.x, 0, move.z);
		}
	}
}

int Game::GetPresentsCollected() {
	int presentsCollected = 0;

	for (int i = 0; i < presents.size(); i++)
	{
		if (presents[i]->collected == true)
			presentsCollected += 1;
	}
	return presentsCollected;
}

void Game::RunWin() {
	Renderer::GetInst().Draw2DQuadPercSpace(vertexShader, *Textures["Win"], glm::vec2(0.5, 0.5), 0.0f, glm::vec2(0.444, 0.417), guiCamera, 0);
}

void Game::initLevel() {
	Level = new GameObj(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(levelXSize, levelHeight, levelZSize), LevelModel);
	player = new Player(glm::vec2(0, levelZSize / 2), glm::vec2(-90, 0));
	
	float wOff = 20;
	glm::vec3 size = glm::vec3(10, 8, 0);
	enemies.push_back(new Enemy(glm::vec3(0, 20, 0), size, Mariah2Model));

	glm::vec3 sp1 = glm::vec3(levelXSize / 2, 20, 0);
	enemies.push_back(new Enemy(sp1, size, MariahModel, Enemy::PATROL, sp1, glm::vec3(levelXSize / 2, 20, levelZSize - wOff)));
	enemies.push_back(new Enemy(sp1, size, MariahModel, Enemy::PATROL, sp1, glm::vec3(levelXSize / 2, 20, -levelZSize + wOff)));

	glm::vec3 sp2 = glm::vec3(levelXSize - wOff, 20, 0);
	enemies.push_back(new Enemy(sp2, size, MariahModel, Enemy::PATROL, sp2, glm::vec3(levelXSize - wOff, 20, levelZSize - wOff)));
	enemies.push_back(new Enemy(sp2, size, MariahModel, Enemy::PATROL, sp2, glm::vec3(levelXSize - wOff, 20, -levelZSize + wOff)));

	glm::vec3 sp3 = glm::vec3(-levelXSize / 2, 20, 0);
	enemies.push_back(new Enemy(sp3, size, MariahModel, Enemy::PATROL, sp3, glm::vec3(-levelXSize / 2, 20, levelZSize - wOff)));
	enemies.push_back(new Enemy(sp3, size, MariahModel, Enemy::PATROL, sp3, glm::vec3(-levelXSize / 2, 20, -levelZSize + wOff)));
																						  
	glm::vec3 sp4 = glm::vec3(-levelXSize + wOff, 20, 0);								  
	enemies.push_back(new Enemy(sp4, size, MariahModel, Enemy::PATROL, sp4, glm::vec3(-levelXSize + wOff, 20, levelZSize - wOff)));
	enemies.push_back(new Enemy(sp4, size, MariahModel, Enemy::PATROL, sp4, glm::vec3(-levelXSize + wOff, 20, -levelZSize + wOff)));

	glm::vec3 ep6 = glm::vec3(0, 20, levelZSize - wOff);
	enemies.push_back(new Enemy(ep6, size, MariahModel, Enemy::PATROL, glm::vec3(levelXSize - wOff, 20, levelZSize - wOff), ep6));
	enemies.push_back(new Enemy(ep6, size, MariahModel, Enemy::PATROL, glm::vec3(-levelXSize + wOff, 20, levelZSize - wOff), ep6));

	glm::vec3 ep8 = glm::vec3(0, 20, -levelZSize + wOff);
	enemies.push_back(new Enemy(ep8, size, MariahModel, Enemy::PATROL, glm::vec3(levelXSize - wOff, 20, -levelZSize + wOff), ep8));
	enemies.push_back(new Enemy(ep8, size, MariahModel, Enemy::PATROL, glm::vec3(-levelXSize + wOff, 20, -levelZSize + wOff), ep8));


	presents.push_back(new Present(glm::vec3(levelXSize - 30, 15, 0), glm::vec3(10, 10, 0), PresentModel));
	presents.push_back(new Present(glm::vec3(levelXSize - 30, 15, levelZSize - 30), glm::vec3(10, 10, 0), PresentModel));
	presents.push_back(new Present(glm::vec3(levelXSize - 30, 15, -levelZSize + 30), glm::vec3(10, 10, 0), PresentModel));

	presents.push_back(new Present(glm::vec3(-levelXSize + 30, 15, 0), glm::vec3(10, 10, 0), PresentModel));
	presents.push_back(new Present(glm::vec3(-levelXSize + 30, 15, levelZSize - 30), glm::vec3(10, 10, 0), PresentModel));
	presents.push_back(new Present(glm::vec3(-levelXSize + 30, 15, -levelZSize + 30), glm::vec3(10, 10, 0), PresentModel));
}