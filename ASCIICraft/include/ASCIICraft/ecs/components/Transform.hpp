// Transform.hpp
#pragma once

#include <ASCIIgL/util/Logger.hpp>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>

namespace ecs::components {

struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 previousPosition{0.0f, 0.0f, 0.0f};
    // Interpolated position for rendering (set each frame by PhysicsSystem)
    glm::vec3 renderPosition{0.0f, 0.0f, 0.0f};
    // Smooth Y for step-climbing: lags behind position.y and decays toward it
    // over ~5 physics ticks (~167 ms) to give a vanilla-style step-up animation.
    // Only written by PhysicsSystem::Step(); not affected by setPosition().
    float renderY{0.0f};
    bool renderYInitialized{false};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // Cached model matrix and dirty flag are mutable so model() can be const.
    mutable glm::mat4 modelCache{1.0f};
    mutable bool dirty{true};

    // Compute or return cached model matrix (uses physics position — for logic, not rendering)
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

    // Build model matrix from interpolated renderPosition (use this in RenderSystem)
    glm::mat4 getRenderModel() const {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), renderPosition);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }

    // Helpers that mark the transform dirty
    // setPosition keeps renderPosition in sync so entities not managed by PhysicsSystem
    // (e.g. static objects, GUI) always display at their actual position.
    void setPosition(const glm::vec3& p) { position = p; renderPosition = p; dirty = true; }
    void setRotation(const glm::quat& q) { rotation = q; dirty = true; }
    void setScale(const glm::vec3& s) { scale = s; dirty = true; }

    // Convenience mutators for incremental updates
    void translate(const glm::vec3& delta) { position += delta; dirty = true; }
    void rotate(const glm::quat& delta) { rotation = delta * rotation; dirty = true; }
    void rescale(const glm::vec3& factor) { scale *= factor; dirty = true; }
};

inline std::pair<glm::vec3, bool> GetPos(entt::entity ent, entt::registry& registry) {
    // Validate entity
    if (ent == entt::null || !registry.valid(ent)) {
        ASCIIgL::Logger::Error("PlayerFactory::GetPosition: Player entity is invalid or null.");
        return {glm::vec3(0.0f), false};
    }

    // Check component existence
    if (!registry.all_of<components::Transform>(ent)) {
        ASCIIgL::Logger::Error("PlayerFactory::GetPosition: Transform component missing on player entity.");
        return {glm::vec3(0.0f), false};
    }

    // Safe retrieval
    auto* t = registry.try_get<components::Transform>(ent);
    if (!t) {
        ASCIIgL::Logger::Error("PlayerFactory::GetPosition: try_get returned NULL for Transform.");
        return {glm::vec3(0.0f), false};
    }

    return {t->position, true};
}

}