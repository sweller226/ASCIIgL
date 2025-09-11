#pragma once

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/renderer/Vertex.hpp>
#include <memory>

class Model
{
public:
    Model(std::string path);
    Model(std::vector<VERTEX> vertices, std::vector<Texture*> textures);
    ~Model();

    // Move constructor and assignment for PIMPL
    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;

    // Delete copy constructor and assignment (or implement if needed)
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    std::vector<Mesh*> meshes;

private:
    // Forward declaration for PIMPL pattern
    class Impl;
    std::unique_ptr<Impl> pImpl;
};