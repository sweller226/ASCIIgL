#include <ASCIIgL/renderer/VertexShader.hpp>
#include <ASCIIgL/renderer/Vertex.hpp>

#include <tbb/parallel_for_each.h>

// UpdateMVP
void Vertex_Shader::UpdateMVP() {
    GLmvp = GLproj * GLview * GLmodel;
}

// Getters
const glm::mat4& Vertex_Shader::GetModel() const { 
    return GLmodel; 
}

const glm::mat4& Vertex_Shader::GetView() const { 
    return GLview; 
}

const glm::mat4& Vertex_Shader::GetProj() const { 
    return GLproj; 
}

const glm::mat4& Vertex_Shader::GetMVP() const { 
    return GLmvp; 
}

// Setters
void Vertex_Shader::SetModel(const glm::mat4& model) { 
    GLmodel = model; 
    UpdateMVP(); 
}

void Vertex_Shader::SetView(const glm::mat4& view) { 
    GLview = view; 
    UpdateMVP(); 
}

void Vertex_Shader::SetProj(const glm::mat4& proj) { 
    GLproj = proj; 
    UpdateMVP(); 
}

void Vertex_Shader::SetMatrices(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj) {
    GLmodel = model;
    GLview = view;
    GLproj = proj;
    UpdateMVP();
}

// Vertex transformation
void Vertex_Shader::GLUse(VERTEX& vertice) const {
    vertice.SetXYZW(GLmvp * vertice.GetXYZW());
}

void Vertex_Shader::GLUseBatch(std::vector<VERTEX>& vertices) const {
    if (vertices.empty()) return;
    
    const size_t vertex_count = vertices.size();
    
    if (vertex_count < 2000) {
        for (VERTEX& vertex : vertices) {
            GLUse(vertex);
        }
        return;
    }
    
    tbb::parallel_for_each(vertices.begin(), vertices.end(),
        [this](VERTEX& vertex) {
            vertex.SetXYZW(GLmvp * vertex.GetXYZW());
        });
}