#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Collision {
	bool DoesPointLineCol(glm::vec2 p, glm::vec2 lineStart, glm::vec2 lineEnd); // gets if a point is colliding with a line
	bool DoesPointCircleCol(glm::vec2 p, glm::vec2 c, float r); // gets if a point is colliding with a circle
	bool DoesAABBCol(const glm::vec2& minA, const glm::vec2& maxA, const glm::vec2& minB, const glm::vec2& maxB); // gets if two axis-aligned bounding boxes are colliding
	bool DoesLineCircleCol(const glm::vec2& c, float r, const glm::vec2& lineStart, const glm::vec2& lineEnd); // returns glm::vec3(isColl, closestPointOnCircleEdge)
	
	glm::vec3 WhereLinePlaneCol(const glm::vec3& planeN, const glm::vec3& planeP, const glm::vec3& lineStart, const glm::vec3& lineEnd); // gets where the line meets the plane
};