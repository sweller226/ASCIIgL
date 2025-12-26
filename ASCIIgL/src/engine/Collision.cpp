#include <ASCIIgL/engine/Collision.hpp>

#include <ASCIIgL/util/MathUtil.hpp>

namespace ASCIIgL {

bool Collision::DoesPointLineCol(glm::vec2 p, glm::vec2 lineStart, glm::vec2 lineEnd)
{

	// get distance from the point to the two ends of the line
	float d1 = glm::distance(p, lineStart);
	float d2 = glm::distance(p, lineEnd);

	// get the length of the line
	float lineLen = glm::distance(lineStart, lineEnd);

	// since floats are so minutely accurate, add
	// a little buffer zone that will give collision
	float buffer = 0.1;    // higher # = less accurate

	// if the two distances are equal to the line's 
	// length, the point is on the line!
	// note we use the buffer here to give a range, 
	// rather than one #
	if (d1 + d2 >= lineLen - buffer && d1 + d2 <= lineLen + buffer) {
		return true;
	}
	return false;
}

bool Collision::DoesLineCircleCol(const glm::vec2& c, float r, const glm::vec2& lineStart, const glm::vec2& lineEnd)
{
	// get length of the line
	float len = glm::distance(lineStart, lineEnd);

	// get dot product of the line and circle
	float dot = (((c.x - lineStart.x) * (lineEnd.x - lineStart.x)) + ((c.y - lineStart.y) * (lineEnd.y - lineStart.y))) / pow(len, 2);

	// find the closest point on the line
	glm::vec2 closest;
	closest.x = lineStart.x + (dot * (lineEnd.x - lineStart.x));
	closest.y = lineStart.y + (dot * (lineEnd.y - lineStart.y));


	// is this point actually on the line segment?
	// if so keep going, but if not, return false
	bool onSegment = DoesPointLineCol(closest, lineStart, lineEnd);
	if (!onSegment) return false;

	// get distance to closest point
	float distance = glm::distance(closest, c);

	// if point is in the circle, line collides with circle
	if (distance <= r) {
		return true;
	}
	return false;
}

bool Collision::DoesPointCircleCol(glm::vec2 p, glm::vec2 c, float r)
{
	// if distance between centres is less than r, point is in circle
	return glm::distance(p, c) < r;
}

glm::vec3 Collision::WhereLinePlaneCol(const glm::vec3& planeN, const glm::vec3& planeP, const glm::vec3& lineStart, const glm::vec3& lineEnd)
{
    glm::vec3 normPlaneN = glm::normalize(planeN);
    float planeD = -glm::dot(normPlaneN, planeP);
    float ad = glm::dot(lineStart, normPlaneN);
    float bd = glm::dot(lineEnd, normPlaneN);

    float denom = bd - ad;
    if (fabs(denom) < 1e-6f) {
        // Line is parallel to plane, no intersection
        return glm::vec3(0, 0, 0); // Or use another sentinel value if needed
    }

    float t = (-planeD - ad) / denom;

    glm::vec3 line = lineEnd - lineStart;
    glm::vec3 lineIntersect = line * t;

    return lineStart + lineIntersect;
}

bool Collision::DoesAABBCol(const glm::vec2& minA, const glm::vec2& maxA, const glm::vec2& minB, const glm::vec2& maxB)
{
    // If one rectangle is on left side of other
    if (maxA.x < minB.x || minA.x > maxB.x) return false;
    // If one rectangle is above other
    if (maxA.y < minB.y || minA.y > maxB.y) return false;
    return true;
}

} // namespace ASCIIgL