#pragma once

#include <glm/glm.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/Material.hpp>

namespace events {

/// Describes a request to spawn one or more particles.
struct ParticleSpawnEvent {
    glm::vec3 origin   = {};
    glm::vec3 velocity = {};
    float     damping  = 0.0f;
    float     lifetime = 1.0f;
    int       count    = 1;
    glm::vec3 scale = glm::vec3(0.125f, 0.125f, 0.125f);

    std::shared_ptr<ASCIIgL::Mesh>     mesh;
    std::shared_ptr<ASCIIgL::Material> material;
};

}