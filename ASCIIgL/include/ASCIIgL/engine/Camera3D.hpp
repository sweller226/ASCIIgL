#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera3D // This is an inbuilt camera 3d class that primarily holds a view mat and a perspective proj mat to make a 3d camera
{
private:

public:
	glm::vec3 pos;
	glm::mat4 view; // this holds the view matrix (the view matrix is a glm::lookat matrix which is used for cameras)
	glm::mat4 proj; // this holds the perspective projection matrix

	float fov; // field of view for the camera, used in the perspective projection matrix

	// rotations of the camera
	float yaw;
	float pitch;
	float aspect;

	// clipping planes on the z axis for the camera
	float zNear;
	float zFar;

	// screen dimensions for aspect ratio calculation
	unsigned int screenWidth;
	unsigned int screenHeight;

	// constructor and destructor
	Camera3D(glm::vec3 Pposition, float Pfov, float Paspect, glm::vec2 yawPitch, float PzNear, float PzFar);
	~Camera3D();

	// each of these output 3d vectors in the front, back, right and left direction
	glm::vec3 getCamFront() const;
	glm::vec3 getCamBack() const;
	glm::vec3 getCamRight() const;
	glm::vec3 getCamLeft() const;

	glm::vec3 getCamFrontNoY() const;
	glm::vec3 getCamBackNoY() const;
	glm::vec3 getCamRightNoY() const;
	glm::vec3 getCamLeftNoY() const;

	void setCamPos(glm::vec3 Pposition);
	void setCamDir(float Pyaw, float Ppitch, float Ppitch_clamp = 89.9f); // this sets the camera dir using angles (not vectors)
	void setCamDir(glm::vec3 dir, float Ppitch_clamp = 89.9f);
	void setScreenDimensions(unsigned int width, unsigned int height); // this updates screen dimensions and recalculates projection matrix
	void recalculateViewMat(); // recalculates the view matrix using the cameras euler angles and position
	void recalculateProjMat(); // recalculates the projection matrix using current screen dimensions and aspect ratio
};