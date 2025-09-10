#include <ASCIIgL/renderer/Vertex.hpp>

Vertex::Vertex() : data{0, 0, 0, 0, 0, 0, 0, 0, 0, 0} {}

Vertex::Vertex(const std::array<float, 10>& indata) : data(indata) {}

Vertex::Vertex(const Vertex& t) : data(t.data) {}

glm::vec2 Vertex::GetXY() const { return glm::vec2(data[0], data[1]); }
glm::vec3 Vertex::GetXYZ() const { return glm::vec3(data[0], data[1], data[2]); }
glm::vec4 Vertex::GetXYZW() const { return glm::vec4(data[0], data[1], data[2], data[3]); }
glm::vec3 Vertex::GetUVW() const { return glm::vec3(data[4], data[5], data[6]); }
glm::vec3 Vertex::GetNXYZ() const { return glm::vec3(data[7], data[8], data[9]); }

float Vertex::X() const { return data[0]; }
float Vertex::Y() const { return data[1]; }
float Vertex::Z() const { return data[2]; }
float Vertex::W() const { return data[3]; }
float Vertex::U() const { return data[4]; }
float Vertex::V() const { return data[5]; }
float Vertex::UVW() const { return data[6]; }
float Vertex::NX() const { return data[7]; }
float Vertex::NY() const { return data[8]; }
float Vertex::NZ() const { return data[9]; }

void Vertex::SetXYZ(const glm::vec3 inXYZ) {
    data[0] = inXYZ.x; data[1] = inXYZ.y; data[2] = inXYZ.z;
}
void Vertex::SetXYZW(const glm::vec4 inXYZW) {
    data[0] = inXYZW.x; data[1] = inXYZW.y; data[2] = inXYZW.z; data[3] = inXYZW.w;
}
void Vertex::SetUV(const glm::vec2 inUV) {
    data[4] = inUV.x; data[5] = inUV.y;
}
void Vertex::SetUVW(const glm::vec3 inUVW) {
    data[4] = inUVW.x; data[5] = inUVW.y; data[6] = inUVW.z;
}
void Vertex::SetNXYZ(const glm::vec3 inNXYZ) {
    data[7] = inNXYZ.x; data[8] = inNXYZ.y; data[9] = inNXYZ.z;
}