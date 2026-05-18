#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIIgL/util/EventBus.hpp>

#include <ASCIICraft/events/SoundEvents.hpp>

namespace ecs::systems {

class SoundSystem : public ISystem {
public:
    SoundSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);
    ~SoundSystem();

    void Update() override;

    bool IsMusicPlaying() const;

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

    SoundBuffer& LoadSoundId(const std::string& soundId);
    SoundBuffer& LoadOggByPath(const std::string& path);
    SoundBuffer  DecodeOgg(const std::string& path);

    /// Returns 0 if no SFX voice could be acquired (at cap and none to steal).
    ALuint AcquireSource();
    void   RecycleFinishedSources();

    static constexpr int MAX_SFX_VOICES = 32;

    entt::registry&    m_registry;
    ASCIIgL::EventBus& m_eventBus;

    ALCdevice*  m_device  = nullptr;
    ALCcontext* m_context = nullptr;

    std::unordered_map<std::string, SoundBuffer> m_buffers;
    std::vector<ALuint>                          m_sources;

    ALuint m_musicSource = 0;
};

} // namespace ecs::systems
