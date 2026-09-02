#include <ASCIICraft/ecs/systems/sound/MusicSystem.hpp>

#include <ASCIICraft/ecs/systems/sound/SoundSystem.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIICraft/events/SoundEvents.hpp>
#include <ASCIICraft/util/RNG.hpp>

namespace ecs::systems {

MusicSystem::MusicSystem(ASCIIgL::EventBus& eventBus, const SoundSystem& soundSystem)
    : m_eventBus(eventBus)
    , m_soundSystem(soundSystem)
{}

void MusicSystem::Update() {
    PROFILE_SCOPE("Music.Update");

    if (m_trackPending) {
        // IsMusicActive() covers the whole life of a track - opening, decoding,
        // and draining the last queued buffers - so it is true from the frame
        // SoundSystem handles our event onwards. No grace period needed.
        if (m_soundSystem.IsMusicActive()) {
            return;
        }

        OnMusicTrackFinished();
        return;
    }

    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    if (m_musicCooldown > 0.0f) {
        m_musicCooldown -= dt;
        return;
    }

    DispatchNextTrack();
}

void MusicSystem::OnMusicTrackFinished() {
    static util::RNG s_rng;
    m_musicCooldown = s_rng.NextFloat(NEXT_TRACK_FLOOR, NEXT_TRACK_CEIL);
    m_trackPending = false;
    ASCIIgL::Logger::Infof("[MusicSystem] Track finished. Next track in %.0fs", m_musicCooldown);
}

void MusicSystem::DispatchNextTrack() {
    static util::RNG s_rng;

    int index = 0;
    do {
        index = s_rng.NextInt(0, static_cast<int>(MUSIC_TRACKS.size()) - 1);
    } while (index == m_lastTrackIndex && MUSIC_TRACKS.size() > 1);
    m_lastTrackIndex = index;

    const std::string& soundId = MUSIC_TRACKS[index];
    ASCIIgL::Logger::Infof("[MusicSystem] Dispatching music track: %s", soundId.c_str());

    m_eventBus.emit(events::PlayMusicEvent{soundId, 0.5f});
    m_trackPending = true;
}

} // namespace ecs::systems
