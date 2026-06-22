#pragma once

#include <memory>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace ASCIIgL { class Mesh; }

namespace ecs::components {

/// Visual data for an item definition entity.
/// Kept separate from Renderable so prototype entities are not
/// processed by the render system.
struct ItemVisual {
    std::shared_ptr<ASCIIgL::Mesh> mesh;
    std::shared_ptr<ASCIIgL::Mesh> droppedMesh;
    bool is2DIcon = false;
};

/// Local transform for 3D block item meshes drawn in inventory / GUI slots.
/// Applied on top of the slot cell placement (center, size, layer).
/// Rotation matches Minecraft item JSON display order: X, then Y, then Z (degrees).
struct ItemGuiMeshTransform {
    glm::vec3 location{0.0f, 0.0f, 0.0f};
    glm::vec3 rotationDegrees{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    /// Dominant block item pose
    static ItemGuiMeshTransform DefaultBlockThirdPerson() {
        ItemGuiMeshTransform t;
        t.location = glm::vec3(0.0f, 0.0f, -5.0f);
        t.rotationDegrees = glm::vec3(-25.0f, -45.0f, 0.0f);
        t.scale = glm::vec3(0.65f);
        return t;
    }

    /// Upright torch icon — thin model reads small at the default isometric pose.
    static ItemGuiMeshTransform DefaultTorch() {
        ItemGuiMeshTransform t;
        t.rotationDegrees = glm::vec3(0.0f);
        t.scale = glm::vec3(0.85f);
        return t;
    }

    glm::mat4 getModel() const {
        constexpr float kDisplayUnitsPerBlock = 16.0f;
        // Minecraft display order: translation, rotation (X→Y→Z), scale.
        glm::mat4 m(1.0f);
        m = glm::translate(m, location / kDisplayUnitsPerBlock);
        m = glm::rotate(m, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, scale);
        return m;
    }
};

/// Local transform for held-item viewmodel meshes (display.firstperson).
/// Separate from ItemGuiMeshTransform — GUI third-person pose ≠ first-person pose.
struct ItemHeldMeshTransform {
    glm::vec3 location{0.0f, 0.0f, 0.0f};
    glm::vec3 rotationDegrees{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    /// Vanilla block-item first-person pose (display.firstperson default for block parents).
    static ItemHeldMeshTransform DefaultBlockFirstPerson() {
        ItemHeldMeshTransform t;
        t.location = glm::vec3(1.0f, 1.0f, 0.0f);
        t.rotationDegrees = glm::vec3(0.0f, 45.0f, 0.0f);
        t.scale = glm::vec3(0.45f);
        return t;
    }

    /// Generated flat-item pose from models/item/*.json display.firstperson.
    static ItemHeldMeshTransform DefaultGeneratedFirstPerson() {
        ItemHeldMeshTransform t;
        t.location = glm::vec3(-0.5f, 3.0f, 1.0f);
        t.rotationDegrees = glm::vec3(-10.0f, -65.0f, 10.0f);
        t.scale = glm::vec3(0.6f);
        return t;
    }

    /// iron_sword.json display.firstperson.
    static ItemHeldMeshTransform DefaultToolFirstPerson() {
        ItemHeldMeshTransform t;
        t.location = glm::vec3(-1.0f, 2.3f, 1.0f);
        t.rotationDegrees = glm::vec3(0.0f, 108.0f, 65.0f);
        t.scale = glm::vec3(0.7f);
        return t;
    }

    static ItemHeldMeshTransform DefaultTorchFirstPerson() {
        ItemHeldMeshTransform t;
        t.location = glm::vec3(-0.5f, 3.0f, 0.0f);
        t.rotationDegrees = glm::vec3(0.0f, 45.0f, 0.0f);
        t.scale = glm::vec3(0.8f);
        return t;
    }

    glm::mat4 getModel() const {
        constexpr float kDisplayUnitsPerBlock = 16.0f;
        glm::mat4 m(1.0f);
        m = glm::translate(m, location / kDisplayUnitsPerBlock);
        m = glm::rotate(m, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, scale);
        return m;
    }
};

} // namespace ecs::components
