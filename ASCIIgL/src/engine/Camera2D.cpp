#include <ASCIIgL/engine/Camera2D.hpp>

namespace ASCIIgL {

Camera2D::Camera2D(glm::vec2 Pposition, unsigned int SCR_WIDTH, unsigned int SCR_HEIGHT)
	: position(Pposition), screenWidth(SCR_WIDTH), screenHeight(SCR_HEIGHT)
{
	recalculateProjMat(); // making the projection matrix
	recalculateViewMat(); // calculating the view mat using the class variables
}

Camera2D::~Camera2D()
{
	// unused destructor
}

void Camera2D::setCamPos(glm::vec2 Pposition)
{
	position = Pposition; // setting new position
	recalculateViewMat(); // recalculating view mat with the new position
}

void Camera2D::recalculateViewMat()
{
	view = glm::translate(glm::mat4(1.0f), glm::vec3(-position, 0.0f));
}

void Camera2D::setScreenDimensions(unsigned int width, unsigned int height)
{
	screenWidth = width;
	screenHeight = height;
	recalculateProjMat(); // recalculate projection matrix with new dimensions
}

void Camera2D::recalculateProjMat()
{
	// Avoid zero dimensions (would produce invalid/undefined NDC). Ortho maps [0,w] x [0,h] to NDC [-1,1].
	float w = static_cast<float>(screenWidth > 0u ? screenWidth : 1u);
	float h = static_cast<float>(screenHeight > 0u ? screenHeight : 1u);
	proj = glm::ortho(0.0f, w, h, 0.0f, -100.0f, 100.0f);
}

} // namespace ASCIIgL