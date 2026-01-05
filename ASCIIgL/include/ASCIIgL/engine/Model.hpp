#pragma once

#include <memory>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Texture.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

class Model
{
public:
    Model(std::string path);
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

} // namespace ASCIIgL