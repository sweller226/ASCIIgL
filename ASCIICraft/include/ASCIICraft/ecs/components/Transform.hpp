// Transform.hpp
#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ecs::components {

struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // Cached model matrix and dirty flag are mutable so model() can be const.
    mutable glm::mat4 modelCache{1.0f};
    mutable bool dirty{true};

    // Compute or return cached model matrix
    glm::mat4 getModel() const {
        if (dirty) {
            glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 r = glm::mat4_cast(rotation);
            glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
            modelCache = t * r * s;
            dirty = false;
        }
        return modelCache;
    }

    // Helpers that mark the transform dirty
    void setPosition(const glm::vec3& p) { position = p; dirty = true; }
    void setRotation(const glm::quat& q) { rotation = q; dirty = true; }
    void setScale(const glm::vec3& s) { scale = s; dirty = true; }

    // Convenience mutators for incremental updates
    void translate(const glm::vec3& delta) { position += delta; dirty = true; }
    void rotate(const glm::quat& delta) { rotation = delta * rotation; dirty = true; }
    void rescale(const glm::vec3& factor) { scale *= factor; dirty = true; }
};

}