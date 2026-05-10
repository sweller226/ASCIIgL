#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/events/SoundEvents.hpp>

namespace ecs::systems {
    
// ============================================================================
// SoundSystem
// ============================================================================
class SoundSystem : public ISystem {
public:
    SoundSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);
    ~SoundSystem();

    void Update() override;  // reclaims finished sources, updates listener position

private:
    struct SoundBuffer {
        ALuint alBuffer   = 0;
        int    channels   = 0;
        int    sampleRate = 0;
    };

    // ── OpenAL lifecycle ─────────────────────────────────────────────────────
    void InitOpenAL();
    void ShutdownOpenAL();

    // ── Event handler ─────────────────────────────────────────────────────────
    void OnPlaySound(const events::PlaySoundEvent& event);

    // ── Asset loading ─────────────────────────────────────────────────────────
    // GetOrLoad  — looks up by dot-separated soundId, converts to path internally
    SoundBuffer& GetOrLoad(const std::string& soundId);
    // LoadOggByPath — loads directly from a file path, caches by that path
    SoundBuffer& LoadOggByPath(const std::string& path);
    SoundBuffer  DecodeOgg(const std::string& path);

    // ── Source pool ───────────────────────────────────────────────────────────
    ALuint AcquireSource();
    void   RecycleFinishedSources();

    // ── Music ─────────────────────────────────────────────────────────────────
    void UpdateMusic(float deltaTime);

    static inline const std::vector<std::string> MUSIC_TRACKS = {
        "res/sounds/music/game/calm1.ogg",
        "res/sounds/music/game/calm2.ogg",
        "res/sounds/music/game/calm3.ogg",
        "res/sounds/music/game/hal1.ogg",
        "res/sounds/music/game/hal2.ogg",
        "res/sounds/music/game/hal3.ogg",
        "res/sounds/music/game/hal4.ogg",
        "res/sounds/music/game/nuance1.ogg",
        "res/sounds/music/game/nuance2.ogg",
        "res/sounds/music/game/piano1.ogg",
        "res/sounds/music/game/piano2.ogg",
        "res/sounds/music/game/piano3.ogg",
    };

    ALuint m_musicSource    = 0;
    int    m_lastTrackIndex = -1;    // prevents same track twice in a row
    float  m_musicCooldown  = 10.0f; // first track plays after 10 seconds

    // ── Members ───────────────────────────────────────────────────────────────
    entt::registry&    m_registry;
    ASCIIgL::EventBus& m_eventBus;

    ALCdevice*  m_device  = nullptr;
    ALCcontext* m_context = nullptr;

    std::unordered_map<std::string, SoundBuffer> m_buffers;  // path/soundId → buffer
    std::vector<ALuint>                          m_sources;  // pool of AL sources
};

} // namespace ecs::systems