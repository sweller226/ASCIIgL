#pragma once

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex {
    std::array<float, 10> data = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    Vertex();
    Vertex(const std::array<float, 10>& indata);
    Vertex(const Vertex& t);

    // Fast accessors
    glm::vec2 GetXY() const;
    glm::vec3 GetXYZ() const;
    glm::vec4 GetXYZW() const;
    glm::vec3 GetUVW() const;
    glm::vec3 GetNXYZ() const;

    // Short getters for every coordinate
    float X() const;
    float Y() const;
    float Z() const;
    float W() const;
    float U() const;
    float V() const;
    float UVW() const;
    float NX() const;
    float NY() const;
    float NZ() const;

    // Fast mutators
    void SetXYZ(const glm::vec3 inXYZ);
    void SetXYZW(const glm::vec4 inXYZW);
    void SetUV(const glm::vec2 inUV);
    void SetUVW(const glm::vec3 inUVW);
    void SetNXYZ(const glm::vec3 inNXYZ);
};

using VERTEX = Vertex;