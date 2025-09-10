#include <ASCIIgL/util/MathUtil.hpp>

#include <algorithm>

glm::vec3 MathUtil::CalcNormal(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const bool out)
{
	glm::vec3 normal;

	glm::vec3 U = p2 - p1;
	glm::vec3 V = p3 - p1;

	if (out == true)
		normal = glm::cross(U, V);
	else
		normal = glm::cross(V, U);
	return glm::normalize(normal);
}


std::pair<glm::vec2, glm::vec2> MathUtil::ComputeBoundingBox(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2) {
    glm::vec2 minPt = glm::min(glm::min(p0, p1), p2);
    glm::vec2 maxPt = glm::max(glm::max(p0, p1), p2);
    return { minPt, maxPt };
}

unsigned int MathUtil::FloorToEven(unsigned int number) {
	if (number % 2 == 1) {  // If the number is odd
		return number - 1;   // Subtract 1 to make it even
	}
	return number;          // If already even, return as is
}