#include <ASCIIgL/engine/Model.hpp>
#include <ASCIIgL/engine/Logger.hpp>

#include <tinyobjloader/tiny_obj_loader.h>
#include <iostream>
#include <string>

// PIMPL Implementation class that contains all tinyobjloader-related code
class Model::Impl
{
public:
    std::string directory;
    std::vector<Texture*> textures_loaded;

    void loadModel(std::string path, std::vector<Mesh*>& meshes);
    Mesh* processMesh(const tinyobj::attrib_t& attrib, const tinyobj::shape_t& shape, const std::vector<tinyobj::material_t>& materials);
    std::vector<Texture*> loadMaterialTextures(const tinyobj::material_t& material);
};

void Model::Impl::loadModel(std::string path, std::vector<Mesh*>& meshes)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

    if (!warn.empty()) {
        Logger::Warning("TINYOBJ: " + warn);
    }

    if (!err.empty()) {
        Logger::Error("TINYOBJ: " + err);
    }

    if (!ret) {
        Logger::Error("TINYOBJ: Failed to load model: " + path);
        return;
    }

    directory = path.substr(0, path.find_last_of('/'));
    
    // Process each shape (mesh) in the model
    for (const auto& shape : shapes) {
        meshes.push_back(processMesh(attrib, shape, materials));
    }
}

Mesh* Model::Impl::processMesh(const tinyobj::attrib_t& attrib, const tinyobj::shape_t& shape, const std::vector<tinyobj::material_t>& materials)
{
    std::vector<VERTEX> vertices;
    std::vector<Texture*> textures;

    // Loop over faces (polygons)
    size_t index_offset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
        int fv = shape.mesh.num_face_vertices[f];

        // Loop over vertices in the face (should be 3 for triangulated mesh)
        for (size_t v = 0; v < fv; v++) {
            // Access to vertex
            tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
            
            VERTEX vertex;

            // Position
            if (idx.vertex_index >= 0) {
                vertex.SetXYZW(glm::vec4(
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2],
                    1.0f
                ));
            }

            // Normal
            if (idx.normal_index >= 0) {
                vertex.SetNXYZ(glm::vec3(
                    attrib.normals[3 * idx.normal_index + 0],
                    attrib.normals[3 * idx.normal_index + 1],
                    attrib.normals[3 * idx.normal_index + 2]
                ));
            }

            // Texture coordinates
            if (idx.texcoord_index >= 0) {
                vertex.SetUV(glm::vec2(
                    attrib.texcoords[2 * idx.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * idx.texcoord_index + 1] // Flip V coordinate
                ));
            } else {
                vertex.SetUV(glm::vec2(0.0f, 0.0f));
            }

            vertices.push_back(vertex);
        }
        index_offset += fv;

        // Load material textures for this face
        if (!materials.empty() && shape.mesh.material_ids[f] >= 0) {
            const auto& material = materials[shape.mesh.material_ids[f]];
            std::vector<Texture*> materialTextures = loadMaterialTextures(material);
            textures.insert(textures.end(), materialTextures.begin(), materialTextures.end());
        }
    }

    return new Mesh(vertices, textures);
}

std::vector<Texture*> Model::Impl::loadMaterialTextures(const tinyobj::material_t& material)
{
    std::vector<Texture*> textures;
    
    // Load diffuse texture
    if (!material.diffuse_texname.empty()) {
        std::string texturePath = directory + "/" + material.diffuse_texname;
        
        // Check if texture was loaded before
        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (textures_loaded[j]->GetFilePath() == texturePath) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }
        
        if (!skip) {
            Texture* texture = new Texture(texturePath, "texture_diffuse");
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }
    
    // Load specular texture
    if (!material.specular_texname.empty()) {
        std::string texturePath = directory + "/" + material.specular_texname;
        
        // Check if texture was loaded before
        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (textures_loaded[j]->GetFilePath() == texturePath) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }
        
        if (!skip) {
            Texture* texture = new Texture(texturePath, "texture_specular");
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }

    return textures;
}

// Model public interface implementation
Model::Model(std::string path) : pImpl(std::make_unique<Impl>())
{
    pImpl->loadModel(path, meshes);
}

Model::Model(std::vector<VERTEX> vertices, std::vector<Texture*> textures) : pImpl(std::make_unique<Impl>())
{
    meshes.push_back(new Mesh(vertices, textures));
}

Model::~Model() = default;

Model::Model(Model&& other) noexcept : meshes(std::move(other.meshes)), pImpl(std::move(other.pImpl))
{
}

Model& Model::operator=(Model&& other) noexcept
{
    if (this != &other)
    {
        meshes = std::move(other.meshes);
        pImpl = std::move(other.pImpl);
    }
    return *this;
}