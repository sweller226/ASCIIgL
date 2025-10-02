#pragma once

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex {
    std::array<float, 7> data = {0, 0, 0, 0, 0, 0, 0};

    Vertex();
    Vertex(const std::array<float, 7>& indata);
    Vertex(const Vertex& t);

    // Fast accessors
    glm::vec2 GetXY() const;
    glm::vec3 GetXYZ() const;
    glm::vec4 GetXYZW() const;
    glm::vec3 GetUVW() const;

    // Short getters for every coordinate
    float X() const;
    float Y() const;
    float Z() const;
    float W() const;
    float U() const;
    float V() const;
    float UVW() const;

    // Fast mutators
    void SetXYZ(const glm::vec3 inXYZ);
    void SetXYZW(const glm::vec4 inXYZW);
    void SetUV(const glm::vec2 inUV);
    void SetUVW(const glm::vec3 inUVW);
};

using VERTEX = Vertex;