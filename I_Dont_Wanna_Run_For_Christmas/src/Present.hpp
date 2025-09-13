#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/GameObj.hpp>

class Present : public GameObj
{
private:

public:
	bool collected = false;

	glm::vec3 rotation = glm::vec3(0, 0, 0);

	Present(glm::vec3 pos, glm::vec3 size, Model* model)
		: GameObj(pos, rotation, size, model)
	{

	}
};