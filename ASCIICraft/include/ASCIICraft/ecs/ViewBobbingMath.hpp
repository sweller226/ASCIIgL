#pragma once

#include <ASCIICraft/ecs/components/Transform.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cmath>

namespace ecs::viewbobbing {

inline float ComputeRenderAlpha(const components::Transform& transform) {
    const glm::vec3 denom = transform.position - transform.previousPosition;
    if (glm::dot(denom, denom) < 1e-10f) {
        return 1.0f;
    }

    const glm::vec3 numer = transform.renderPosition - transform.previousPosition;
    return glm::clamp(glm::dot(numer, denom) / glm::dot(denom, denom), 0.0f, 1.0f);
}

inline glm::mat4 BuildMinecraftBobMatrix(
    float walkPhase,
    float cameraYaw,
    float cameraPitch,
    float intensity
) {
    cameraYaw *= intensity;
    cameraPitch *= intensity;

    const float phase = walkPhase * glm::pi<float>();

    const glm::vec3 localTranslate(
        std::sin(phase) * cameraYaw * 0.5f,
        -std::abs(std::cos(phase) * cameraYaw),
        0.0f
    );
    const float rollDegrees = std::sin(phase) * cameraYaw * 3.0f;
    const float pitchBobDegrees = std::abs(std::cos(phase - 0.2f) * cameraYaw) * 5.0f;

    glm::mat4 bob(1.0f);
    bob = glm::translate(bob, localTranslate);
    bob = glm::rotate(bob, glm::radians(rollDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
    bob = glm::rotate(bob, glm::radians(pitchBobDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
    bob = glm::rotate(bob, glm::radians(cameraPitch), glm::vec3(1.0f, 0.0f, 0.0f));
    return bob;
}

} // namespace ecs::viewbobbing
