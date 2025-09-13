#pragma once

#include <ASCIIgL/engine/Model.hpp>

class GameObj // this class just represents a general game object, meant to be inherited and extended
{ // just holds a model, and a position size and rotation
private:

public:
	GameObj(glm::vec3 pos, glm::vec3 rot, glm::vec3 inSize, Model* modelPtr);
	~GameObj();

	Model* model = nullptr;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 size;
};