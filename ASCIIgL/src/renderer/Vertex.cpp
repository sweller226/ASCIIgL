#include <ASCIIgL/renderer/Vertex.hpp>

Vertex::Vertex() : data{0, 0, 0, 0, 0, 0, 0} {}

Vertex::Vertex(const std::array<float, 7>& indata) : data(indata) {}

Vertex::Vertex(const Vertex& t) : data(t.data) {}

glm::vec2 Vertex::GetXY() const { return glm::vec2(data[0], data[1]); }
glm::vec3 Vertex::GetXYZ() const { return glm::vec3(data[0], data[1], data[2]); }
glm::vec4 Vertex::GetXYZW() const { return glm::vec4(data[0], data[1], data[2], data[3]); }
glm::vec2 Vertex::GetUV() const { return glm::vec2(data[4], data[5]); }

float Vertex::X() const { return data[0]; }
float Vertex::Y() const { return data[1]; }
float Vertex::Z() const { return data[2]; }
float Vertex::W() const { return data[3]; }
float Vertex::U() const { return data[4]; }
float Vertex::V() const { return data[5]; }
float Vertex::InvW() const { return data[6]; }

void Vertex::SetXY(const glm::vec2 inXY) {
    data[0] = inXY.x; data[1] = inXY.y;
}
void Vertex::SetXYZ(const glm::vec3 inXYZ) {
    data[0] = inXYZ.x; data[1] = inXYZ.y; data[2] = inXYZ.z;
}
void Vertex::SetXYZW(const glm::vec4 inXYZW) {
    data[0] = inXYZW.x; data[1] = inXYZW.y; data[2] = inXYZW.z; data[3] = inXYZW.w;
}
void Vertex::SetUV(const glm::vec2 inUV) {
    data[4] = inUV.x; data[5] = inUV.y;
}
void Vertex::SetInvW(const float inInvW) {
    data[6] = inInvW;
}
void Vertex::SetW(const float inW) {
    data[3] = inW;
}