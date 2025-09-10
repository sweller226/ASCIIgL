#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera2D // this is an inbuilt 2d camera that uses orthographic projection instead of perspective projection
{
private:
	unsigned int screenWidth;
	unsigned int screenHeight;

public:
	glm::vec2 position;
	glm::mat4 view; // this is a translation matrix instead of a lookat matrix and it takes in the negative of the position vector
	glm::mat4 proj; // this is an orthographic matrix instead of a projection matrix because 2d has no depth

	Camera2D(glm::vec2 Pposition, unsigned int SCR_WIDTH, unsigned int SCR_HEIGHT);
	~Camera2D();

	void setCamPos(glm::vec2 Pposition); // this sets the camera position
	void setScreenDimensions(unsigned int width, unsigned int height); // this updates screen dimensions and recalculates projection matrix
	void recalculateViewMat(); // this resets the view matrix with the
	void recalculateProjMat(); // this recalculates the projection matrix with current screen dimensions
};