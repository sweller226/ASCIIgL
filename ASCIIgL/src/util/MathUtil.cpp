#include <ASCIIgL/util/MathUtil.hpp>

#include <algorithm>

namespace ASCIIgL {

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

glm::mat4 MathUtil::CalcModelMatrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size) {
    // position is the bottom right corner if this is a 2D object
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, -0.5f * size.z));
	model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch (X-axis)
	model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw (Y-axis)
	model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z-axis)
	model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.5f * size.z));
	model = glm::scale(model, size);

	return model;
}

glm::mat4 MathUtil::CalcModelMatrix(const glm::vec3& position, const float rotation, const glm::vec3& size) {
    // position is the bottom right corner if this is a 2D object
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, -0.5f * size.z));
	model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z-axis)
	model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.5f * size.z));
	model = glm::scale(model, size);

	return model;
}

int MathUtil::FloorDivNegInf(int n, int d) {
	return n >= 0 ? n / d : (n - d + 1) / d;
}

} // namespace ASCIIgL