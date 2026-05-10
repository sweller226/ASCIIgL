#pragma once

#include <string>
#include <entt/entt.hpp> 

namespace events {

struct PlaySoundEvent {
    std::string soundId;    // e.g. "block.grass.step"
    entt::entity entity;    // for positional audio, use entt::null for 2D/UI sounds
    float volume = 1.0f;
    float pitch  = 1.0f;
};

}