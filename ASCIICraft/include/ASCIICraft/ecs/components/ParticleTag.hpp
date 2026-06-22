#pragma once

namespace ecs::components {

/// Tag component to mark transient particle entities.
/// Used by ParticleSystem for particle-only cleanup.
struct ParticleTag {};

} // namespace ecs::components
