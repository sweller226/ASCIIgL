#include <ASCIIgL/renderer/cpu/VertexShaderCPU.hpp>

#include <tbb/parallel_for_each.h>

namespace ASCIIgL {

// UpdateMVP
void Vertex_Shader_CPU::UpdateMVP() {
    _mvp = _proj * _view * _model;
}

// Getters
const glm::mat4& Vertex_Shader_CPU::GetModel() const { 
    return _model; 
}

const glm::mat4& Vertex_Shader_CPU::GetView() const { 
    return _view; 
}

const glm::mat4& Vertex_Shader_CPU::GetProj() const { 
    return _proj; 
}

const glm::mat4& Vertex_Shader_CPU::GetMVP() const { 
    return _mvp; 
}

// Setters
void Vertex_Shader_CPU::SetModel(const glm::mat4& model) { 
    _model = model; 
    UpdateMVP(); 
}

void Vertex_Shader_CPU::SetView(const glm::mat4& view) { 
    _view = view; 
    UpdateMVP(); 
}

void Vertex_Shader_CPU::SetProj(const glm::mat4& proj) { 
    _proj = proj; 
    UpdateMVP(); 
}

void Vertex_Shader_CPU::SetMatrices(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj) {
    _model = model;
    _view = view;
    _proj = proj;
    UpdateMVP();
}

// Vertex transformation
void Vertex_Shader_CPU::Use(VertStructs::PosWUVInvW& vertice) const {
    vertice.SetXYZW(_mvp * vertice.GetXYZW());
}

void Vertex_Shader_CPU::UseBatch(std::vector<VertStructs::PosWUVInvW>& vertices) const {
    if (vertices.empty()) return;
    
    const size_t vertex_count = vertices.size();
    
    if (vertex_count < 2000) {
        for (VertStructs::PosWUVInvW& vertex : vertices) {
            Use(vertex);
        }
        return;
    }
    
    tbb::parallel_for_each(vertices.begin(), vertices.end(),
        [this](VertStructs::PosWUVInvW& vertex) {
            vertex.SetXYZW(_mvp * vertex.GetXYZW());
        });
}

} // namespace ASCIIgL