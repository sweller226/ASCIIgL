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
    int  PickTrackIndex();

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

    // Which track opens a session, and how long we wait before it. Empty means "no
    // preference": the first track is picked at random after the usual opening
    // cooldown, like every track after it. A name that is not in MUSIC_TRACKS is
    // ignored the same way (with a warning) rather than dispatched blind, so the
    // no-repeat and cooldown logic always has a real index to work from.
    static inline const std::string FIRST_TRACK = "music.game.piano3"; // "Haggstrom"

    ASCIIgL::EventBus&     m_eventBus;
    const SoundSystem&     m_soundSystem;

    int   m_lastTrackIndex = -1;
    float m_musicCooldown  = 0.0f; // set by the constructor
    bool  m_trackPending   = false;

    static constexpr float NEXT_TRACK_CEIL  = 120.0f;
    static constexpr float NEXT_TRACK_FLOOR = 80.0f;

    static constexpr float FIRST_TRACK_DELAY  = 2.0f;  // when FIRST_TRACK is set
    static constexpr float OPENING_COOLDOWN   = 10.0f; // when it is not
};

} // namespace ecs::systems
