#include <ASCIIgL/engine/Model.hpp>

#include <tinyobjloader/tiny_obj_loader.h>
#include <iostream>
#include <string>
#include <set>

#include <ASCIIgL/engine/Logger.hpp>

// PIMPL Implementation class that contains all tinyobjloader-related code
class Model::Impl
{
public:
    std::string directory;
    std::vector<Texture*> textures_loaded;

    void loadModel(std::string path, std::vector<Mesh*>& meshes);
    Mesh* processMesh(const tinyobj::attrib_t& attrib, const tinyobj::shape_t& shape, const std::vector<tinyobj::material_t>& materials);
    Texture* loadMaterialTextures(const tinyobj::material_t& material);
};

void Model::Impl::loadModel(std::string path, std::vector<Mesh*>& meshes)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Extract directory for material file and texture paths
    directory = path.substr(0, path.find_last_of('/'));
    if (directory.empty()) {
        directory = "."; // Current directory if no path separator found
    }

    // Load OBJ with material file directory specified
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), directory.c_str());

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

    Logger::Info("TINYOBJ: Loaded " + std::to_string(shapes.size()) + " shapes and " + 
                 std::to_string(materials.size()) + " materials");
    
    // Process each shape (mesh) in the model
    for (const auto& shape : shapes) {
        meshes.push_back(processMesh(attrib, shape, materials));
    }
}

Mesh* Model::Impl::processMesh(const tinyobj::attrib_t& attrib, const tinyobj::shape_t& shape, const std::vector<tinyobj::material_t>& materials)
{
    std::vector<VERTEX> vertices;
    Texture* texture = nullptr;

    // Collect unique material IDs for this shape
    std::set<int> uniqueMaterialIds;
    for (size_t f = 0; f < shape.mesh.material_ids.size(); f++) {
        if (shape.mesh.material_ids[f] >= 0) {
            uniqueMaterialIds.insert(shape.mesh.material_ids[f]);
        }
    }

    // Load textures for all materials used by this shape
    for (int materialId : uniqueMaterialIds) {
        if (materialId < materials.size()) {
            const auto& material = materials[materialId];
            Texture* materialTexture = loadMaterialTextures(material);
            if (materialTexture != nullptr) {
                texture = materialTexture;
                break; // Only use the first diffuse texture found
            }
        }
    }

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
    }

    return new Mesh(std::move(vertices), texture);
}

Texture* Model::Impl::loadMaterialTextures(const tinyobj::material_t& material)
{
    Logger::Debug("Loading diffuse texture for material: " + material.name);
    
    // Only load diffuse texture (meshes now support only one texture)
    if (!material.diffuse_texname.empty()) {
        std::string texturePath;
        if (material.diffuse_texname.find("res/") == 0 || material.diffuse_texname[0] == '/') {
            // Already absolute path
            texturePath = material.diffuse_texname;
        } else {
            // Relative path, prepend directory
            texturePath = directory + "/" + material.diffuse_texname;
        }
        Logger::Debug("Loading diffuse texture: " + texturePath);
        
        // Check if texture was loaded before
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (textures_loaded[j]->GetFilePath() == texturePath) {
                Logger::Debug("Reusing previously loaded texture: " + texturePath);
                return textures_loaded[j];
            }
        }
        
        // Load new texture
        try {
            Texture* texture = new Texture(texturePath, "texture_diffuse");
            textures_loaded.push_back(texture);
            Logger::Info("Successfully loaded diffuse texture: " + texturePath);
            return texture;
        } catch (const std::exception& e) {
            Logger::Error("Failed to load diffuse texture: " + texturePath + " - " + e.what());
        }
    } else {
        Logger::Debug("No diffuse texture found for material: " + material.name);
    }

    return nullptr;
}

// Model public interface implementation
Model::Model(std::string path) : pImpl(std::make_unique<Impl>())
{
    pImpl->loadModel(path, meshes);
}

Model::Model(std::vector<VERTEX>&& vertices, Texture* texture) : pImpl(std::make_unique<Impl>())
{
    meshes.push_back(new Mesh(std::move(vertices), texture));
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