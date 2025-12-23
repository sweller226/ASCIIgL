#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <utility>

namespace MathUtil {
	glm::vec3 CalcNormal(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const bool out); // calculates normals given 3 points
	std::pair<glm::vec2, glm::vec2> ComputeBoundingBox(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2); // computes the axis-aligned bounding box for 3 points
	unsigned int FloorToEven(unsigned int number); // floors an integer to the nearest even number

	glm::mat4 CalcModelMatrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size);
    glm::mat4 CalcModelMatrix(const glm::vec3& position, const float rotation, const glm::vec3& size);
};