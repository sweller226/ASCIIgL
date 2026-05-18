#pragma once

#include <string>
#include <vector>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIIgL/util/EventBus.hpp>

namespace ecs::systems {

class SoundSystem;

class MusicSystem : public ISystem {
public:
    MusicSystem(ASCIIgL::EventBus& eventBus, const SoundSystem& soundSystem);

    void Update() override;

private:
    void OnMusicTrackFinished();
    void DispatchNextTrack();

    static inline const std::vector<std::string> MUSIC_TRACKS = {
        "music.game.calm1",
        "music.game.calm2",
        "music.game.calm3",
        "music.game.hal1",
        "music.game.hal2",
        "music.game.hal3",
        "music.game.hal4",
        "music.game.nuance1",
        "music.game.nuance2",
        "music.game.piano1",
        "music.game.piano2",
        "music.game.piano3",
    };

    ASCIIgL::EventBus&     m_eventBus;
    const SoundSystem&     m_soundSystem;

    int   m_lastTrackIndex       = -1;
    float m_musicCooldown        = 10.0f;
    bool  m_trackPending         = false;
    int   m_framesSinceDispatch  = 0;

    static constexpr float NEXT_TRACK_CEIL  = 120.0f;
    static constexpr float NEXT_TRACK_FLOOR = 80.0f;
};

} // namespace ecs::systems
