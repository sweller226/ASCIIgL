#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIIgL/util/EventBus.hpp>

#include <ASCIICraft/events/SoundEvents.hpp>
#include <ASCIICraft/sound/MusicStream.hpp>

namespace ecs::systems {

class SoundSystem : public ISystem {
public:
    SoundSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);
    ~SoundSystem();

    void Update() override;

    /// True while a music track is still going: the stream is open, or its last
    /// buffers are still draining. Deliberately broader than "the AL source says
    /// AL_PLAYING" - a streaming source dips out of AL_PLAYING on any underrun,
    /// and treating that as "finished" would restart the track mid-play.
    bool IsMusicActive() const;

private:
    struct SoundBuffer {
        ALuint alBuffer   = 0;
        int    channels   = 0;
        int    sampleRate = 0;
    };

    void InitOpenAL();
    void ShutdownOpenAL();

    void OnPlaySound(const events::PlaySoundEvent& event);
    void OnPlayMusic(const events::PlayMusicEvent& event);

    /// Registry lookup plus random variant pick. Empty string (already logged)
    /// if the id is unknown or has no paths.
    std::string ResolveSoundPath(const std::string& soundId);

    /// Reclaims played buffers, refills from the decoder, and keeps the source
    /// running. Cheap enough to call every frame; does nothing when idle.
    void PumpMusicStream();

    SoundBuffer& LoadSoundId(const std::string& soundId);
    SoundBuffer& LoadOggByPath(const std::string& path);
    SoundBuffer  DecodeOgg(const std::string& path);

    /// Returns 0 if no SFX voice could be acquired (at cap and none to steal).
    ALuint AcquireSource();

    static constexpr int MAX_SFX_VOICES = 32;

    entt::registry&    m_registry;
    ASCIIgL::EventBus& m_eventBus;

    ALCdevice*  m_device  = nullptr;
    ALCcontext* m_context = nullptr;

    std::unordered_map<std::string, SoundBuffer> m_buffers;
    std::vector<ALuint>                          m_sources;

    // Music is streamed, never cached: a track is 33-50 MB decoded, and the ring
    // below holds ~0.75 s of it at a time. The source and its buffers are created
    // once in InitOpenAL and reused for every track.
    ALuint                                             m_musicSource = 0;
    std::array<ALuint, sound::MusicStream::RING_CHUNKS> m_musicBuffers{};
    std::vector<ALuint>                                m_freeMusicBuffers;
    sound::MusicStream                                 m_musicStream;

    /// Reused across frames so PopChunk can recycle its buffer instead of
    /// allocating one per chunk.
    sound::MusicStream::Chunk m_musicChunk;
};

} // namespace ecs::systems
