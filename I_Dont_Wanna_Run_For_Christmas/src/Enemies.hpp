#pragma once

#include <ASCIIgL/engine/GameObj.hpp>
#include <ASCIIgL/engine/Model.hpp>

class Enemy: public GameObj // this class represents enemies and inherits 
{
private:
	
public:
	glm::vec2 rotation = glm::vec2(0, 0); // default rotation doesn't matter, as all enemies will be billboard enemies (face camera)

	enum MODES // different modes for the mariah carey ai
	{
		CHASE,
		PATROL
	};

	int aiState;

	// patrolling logic, basically enemies can patrol from spot to another, and when they hit a spot, they change direction and go back to the other spot, in a loop
	glm::vec3 patrolDest;
	int destRadius = 10;

	// these just hold where the patrol starts and where it ends
	glm::vec3 patrolStart;
	glm::vec3 patrolEnd;

	Enemy(glm::vec3 pos, glm::vec3 size, Model* model, int state = CHASE, glm::vec3 patrolS = glm::vec3(0,0,0), glm::vec3 patrolE = glm::vec3(0, 0, 0))
		: GameObj(pos, rotation, size, model), aiState(state), patrolDest(patrolS), patrolStart(patrolS), patrolEnd(patrolE)
	{

	}

	// just a simple function to help move the object
	void Move(glm::vec3 move)
	{
		position += move;
	};
};
