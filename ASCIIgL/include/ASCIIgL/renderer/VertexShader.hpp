#pragma once

#include <vector>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/Vertex.hpp>

struct Vertex_Shader {
    glm::mat4 GLmodel = glm::mat4(1.0f);
    glm::mat4 GLview = glm::mat4(1.0f);
    glm::mat4 GLproj = glm::mat4(1.0f);
    glm::mat4 GLmvp = glm::mat4(1.0f); // precomputed MVP matrix

    // Internal helper to update the MVP matrix
    void UpdateMVP();

    // Getters
    const glm::mat4& GetModel() const;
    const glm::mat4& GetView() const;
    const glm::mat4& GetProj() const;
    const glm::mat4& GetMVP() const;

    // Setters
    void SetModel(const glm::mat4& model);
    void SetView(const glm::mat4& view);
    void SetProj(const glm::mat4& proj);
    void SetMatrices(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj);

    // Vertex transformation methods
    void GLUse(VERTEX& vertice) const;
    void GLUseBatch(std::vector<VERTEX>& vertices) const;
};

typedef Vertex_Shader VERTEX_SHADER;