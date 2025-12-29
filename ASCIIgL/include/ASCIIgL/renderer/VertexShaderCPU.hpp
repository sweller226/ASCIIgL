#pragma once

#include <vector>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

struct Vertex_Shader_CPU {
    private: 
    glm::mat4 _proj = glm::mat4(1.0f);
    glm::mat4 _view = glm::mat4(1.0f);
    glm::mat4 _model = glm::mat4(1.0f);
    glm::mat4 _mvp = glm::mat4(1.0f); // precomputed MVP matrix
    
    public:
    
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
    void Use(VertStructs::PosUVW& vertice) const;
    void UseBatch(std::vector<VertStructs::PosUVW>& vertices) const;
};

typedef Vertex_Shader_CPU VERTEX_SHADER_CPU;

} // namespace ASCIIgL