#pragma once

enum class GameMode {
    Survival,
    Spectator  // Spectator mode with noclip
};

namespace ecs::components {

struct PlayerMode {
  GameMode gamemode;
};

}