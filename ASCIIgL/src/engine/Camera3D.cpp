#include <math.h>
#include <algorithm>

#include <ASCIIgL/engine/Camera3D.hpp>

namespace ASCIIgL {

Camera3D::Camera3D(glm::vec3 Pposition, float Pfov, float Paspect, glm::vec2 yawPitch, float PzNear, float PzFar)
	: pos(Pposition), fov(Pfov), pitch(yawPitch.y), yaw(yawPitch.x), aspect(Paspect), zNear(PzNear), zFar(PzFar)
{
	// Calculate initial screen dimensions from aspect ratio (assuming height = 1080 for default)
	screenHeight = 1080;
	screenWidth = (unsigned int)(screenHeight * aspect);
	
	recalculateViewMat(); // this function uses all all class attributes
	recalculateProjMat(); // calculating the perspective projection
}

Camera3D::~Camera3D()
{
	// unused destructor
}

glm::vec3 Camera3D::getCamFront() const
{
	// this function calculates the directional vector of the front of the camera (where the camera is facing)
	glm::vec3 camDir(0.0f, 0.0f, 0.0f);

	// these are all random ass ways to translate euler angles to a directional vector online don't ask me how it works
	camDir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	camDir.y = sin(glm::radians(pitch));
	camDir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	camDir = glm::normalize(camDir); // turning it into a direction instead of a 3d point
	
	return camDir;
}

glm::vec3 Camera3D::getCamBack() const
{
	return -getCamFront();
}

glm::vec3 Camera3D::getCamRight() const
{
	return glm::normalize(glm::cross(getCamFront(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera3D::getCamLeft() const
{
	return -getCamRight();
}

glm::vec3 Camera3D::getCamFrontNoY() const
{
	glm::vec3 front = getCamFront();
	front.y = 0.0f;
	return glm::normalize(front);
}

glm::vec3 Camera3D::getCamBackNoY() const
{
	glm::vec3 back = getCamBack();
	back.y = 0.0f;
	return glm::normalize(back);
}

glm::vec3 Camera3D::getCamRightNoY() const
{
	glm::vec3 right = getCamRight();
	right.y = 0.0f;
	return glm::normalize(right);
}

glm::vec3 Camera3D::getCamLeftNoY() const
{
	glm::vec3 left = getCamLeft();
	left.y = 0.0f;
	return glm::normalize(left);
}

void Camera3D::setCamPos(glm::vec3 Pposition)
{
	pos = Pposition;
	recalculateViewMat();
}

void Camera3D::setCamDir(float Pyaw, float Ppitch, float Ppitch_clamp) {
	Ppitch_clamp = std::min(fabs(Ppitch_clamp), 89.9f); // clamp pitch clamp to avoid gimbal lock

	// this function takes in a yaw and pitch (no roll cus I didn't code that in (really should)) and sets the yaw in pitch to the new yaw and pitch
	yaw = Pyaw;
	pitch = Ppitch;

	if (pitch > Ppitch_clamp)
		pitch = Ppitch_clamp;
	if (pitch < -Ppitch_clamp)
		pitch = -Ppitch_clamp;

	recalculateViewMat();
}

void Camera3D::setCamDir(glm::vec3 dir, float Ppitch_clamp) {
    // Normalize the direction vector first
    dir = glm::normalize(dir);
    
    // Clamp pitch clamp to avoid gimbal lock
    Ppitch_clamp = std::min(fabs(Ppitch_clamp), 89.9f);
    
    // Calculate pitch from the y component
    // pitch = arcsin(y) where y is the normalized direction's y component
    pitch = glm::degrees(asin(dir.y));
    
    // Clamp pitch to prevent gimbal lock
    if (pitch > Ppitch_clamp)
        pitch = Ppitch_clamp;
    if (pitch < -Ppitch_clamp)
        pitch = -Ppitch_clamp;
    
    yaw = glm::degrees(atan2(dir.z, dir.x));
    
    recalculateViewMat();
}

void Camera3D::recalculateViewMat()
{
	view = glm::lookAt(pos, pos + getCamFront(), glm::vec3(0.0, 1.0, 0.0));
}

void Camera3D::setScreenDimensions(unsigned int width, unsigned int height)
{
	screenWidth = width;
	screenHeight = height;
	aspect = (float)width / (float)height; // update aspect ratio
	recalculateProjMat(); // recalculate projection matrix with new aspect ratio
}

void Camera3D::recalculateProjMat()
{
	proj = glm::perspective(glm::radians(fov), aspect, zNear, zFar);
}

void Camera3D::SetFov(float Pfov) {
	fov = Pfov;
	recalculateProjMat();
}

float Camera3D::GetFov() const {
	return fov;
}

float Camera3D::GetAspect() const {
	return aspect;
}

void Camera3D::SetAspect(float Paspect) {
	aspect = Paspect;
	recalculateProjMat();
}

float Camera3D::GetZNear() const {
	return zNear;
}

void Camera3D::SetZNear(float PzNear) {
	zNear = PzNear;
	recalculateProjMat();
}

float Camera3D::GetZFar() const {
	return zFar;
}

void Camera3D::SetZFar(float PzFar) {
	zFar = PzFar;
	recalculateProjMat();
}

float Camera3D::GetYaw() const {
	return yaw;
}

void Camera3D::SetYaw(float Pyaw) {
	yaw = Pyaw;
	recalculateViewMat();
}

float Camera3D::GetPitch() const {
	return pitch;
}

void Camera3D::SetPitch(float Ppitch) {
	pitch = Ppitch;
	recalculateViewMat();
}

} // namespace ASCIIgL