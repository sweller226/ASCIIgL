#pragma once

enum class GameMode {
    Survival,
    Creative,  // Survival movement or toggled flight; instant block breaking
    Spectator  // Spectator mode with noclip
};

namespace ecs::components {

struct PlayerMode {
  GameMode gamemode;
};

}