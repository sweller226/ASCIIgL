#include <ASCIICraft/ecs/systems/SoundSystem.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/util/RNG.hpp>
#include <stdexcept>
#include <string>

namespace ecs::systems {

// ============================================================================
// Constructor / Destructor
// ============================================================================

SoundSystem::SoundSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus)
    : m_registry(registry)
    , m_eventBus(eventBus)
{
    InitOpenAL();
}

SoundSystem::~SoundSystem()
{
    ShutdownOpenAL();
}

// ============================================================================
// Update — called once per frame
// ============================================================================

void SoundSystem::Update()
{
    RecycleFinishedSources();

    float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    UpdateMusic(dt);

    // Update listener position to the first entity with a Transform
    auto view = m_registry.view<components::Transform>();
    if (!view.empty()) {
        auto& transform = view.get<components::Transform>(view.front());
        alListener3f(AL_POSITION,
            transform.position.x,
            transform.position.y,
            transform.position.z);
    }

    // Process all PlaySoundEvents emitted this frame
    for (const auto& event : m_eventBus.view<events::PlaySoundEvent>())
        OnPlaySound(event);
}

// ============================================================================
// Event handler
// ============================================================================

void SoundSystem::OnPlaySound(const events::PlaySoundEvent& event)
{
    SoundBuffer& buf = GetOrLoad(event.soundId);
    if (buf.alBuffer == 0)
        return;

    ALuint source = AcquireSource();

    alSourcei(source, AL_BUFFER, static_cast<ALint>(buf.alBuffer));
    alSourcef(source, AL_GAIN,   event.volume);
    alSourcef(source, AL_PITCH,  event.pitch);

    if (event.entity != entt::null && m_registry.valid(event.entity)) {
        if (m_registry.all_of<components::Transform>(event.entity)) {
            auto& transform = m_registry.get<components::Transform>(event.entity);
            alSource3f(source, AL_POSITION,
                transform.position.x,
                transform.position.y,
                transform.position.z);
            alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
        }
    } else {
        // 2D / UI sound — relative to listener, no distance falloff
        alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSourcei(source,  AL_SOURCE_RELATIVE, AL_TRUE);
    }

    alSourcePlay(source);
}

// ============================================================================
// Asset loading
// ============================================================================

SoundSystem::SoundBuffer& SoundSystem::GetOrLoad(const std::string& soundId)
{
    // Check cache first
    auto it = m_buffers.find(soundId);
    if (it != m_buffers.end())
        return it->second;

    // Convert dot-separated soundId → file path
    // e.g. "block.grass.step" → "res/sounds/block/grass/step.ogg"
    std::string path = "res/sounds/";
    for (char c : soundId)
        path += (c == '.') ? '/' : c;
    path += ".ogg";

    ASCIIgL::Logger::Infof("[SoundSystem] Loading sound: %s -> %s", soundId.c_str(), path.c_str());

    SoundBuffer buf = DecodeOgg(path);
    if (buf.alBuffer == 0)
        ASCIIgL::Logger::Errorf("[SoundSystem] Failed to load sound '%s' from %s", soundId.c_str(), path.c_str());

    auto [inserted, _] = m_buffers.emplace(soundId, buf);
    return inserted->second;
}

SoundSystem::SoundBuffer& SoundSystem::LoadOggByPath(const std::string& path)
{
    // Check cache first — music tracks are cached by their raw path
    auto it = m_buffers.find(path);
    if (it != m_buffers.end())
        return it->second;

    ASCIIgL::Logger::Infof("[SoundSystem] Loading ogg: %s", path.c_str());

    SoundBuffer buf = DecodeOgg(path);
    if (buf.alBuffer == 0)
        ASCIIgL::Logger::Errorf("[SoundSystem] Failed to load ogg from path: %s", path.c_str());

    auto [inserted, _] = m_buffers.emplace(path, buf);
    return inserted->second;
}

SoundSystem::SoundBuffer SoundSystem::DecodeOgg(const std::string& path)
{
    SoundBuffer result;

    int    channels   = 0;
    int    sampleRate = 0;
    short* pcmData    = nullptr;

    int sampleCount = stb_vorbis_decode_filename(
        path.c_str(),
        &channels,
        &sampleRate,
        &pcmData
    );

    if (sampleCount < 0 || pcmData == nullptr) {
        ASCIIgL::Logger::Errorf("[SoundSystem] stb_vorbis failed to decode: %s (error code: %d)", path.c_str(), sampleCount);
        return result; // alBuffer stays 0
    }

    ALenum format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    alGenBuffers(1, &result.alBuffer);
    alBufferData(
        result.alBuffer,
        format,
        pcmData,
        sampleCount * channels * static_cast<int>(sizeof(short)),
        sampleRate
    );

    // Check for OpenAL errors after upload
    ALenum alErr = alGetError();
    if (alErr != AL_NO_ERROR) {
        ASCIIgL::Logger::Errorf("[SoundSystem] alBufferData failed for %s (AL error: %d)", path.c_str(), alErr);
        alDeleteBuffers(1, &result.alBuffer);
        result.alBuffer = 0;
    }

    result.channels   = channels;
    result.sampleRate = sampleRate;

    free(pcmData);
    return result;
}

// ============================================================================
// Music
// ============================================================================

void SoundSystem::UpdateMusic(float deltaTime)
{
    // Tick cooldown — don't play anything while waiting
    if (m_musicCooldown > 0.0f) {
        m_musicCooldown -= deltaTime;
        return;
    }

    // If a source exists, check if it's still playing
    if (m_musicSource != 0) {
        ALint state;
        alGetSourcei(m_musicSource, AL_SOURCE_STATE, &state);

        if (state == AL_PLAYING)
            return; // still going, nothing to do

        // Track finished — start cooldown and clear source for next play
        static util::RNG s_rng;
        m_musicCooldown = s_rng.NextFloat(60.0f, 180.0f);
        m_musicSource   = 0;  // will be re-allocated next play
        ASCIIgL::Logger::Infof("[SoundSystem] Music track finished. Next track in %.0fs", m_musicCooldown);
        return;
    }

    // No source and no cooldown — pick and play a track
    static util::RNG s_rng;
    int index;
    do {
        index = s_rng.NextInt(0, static_cast<int>(MUSIC_TRACKS.size()) - 1);
    } while (index == m_lastTrackIndex && MUSIC_TRACKS.size() > 1);
    m_lastTrackIndex = index;

    const std::string& path = MUSIC_TRACKS[index];
    ASCIIgL::Logger::Infof("[SoundSystem] Playing music track: %s", path.c_str());

    SoundBuffer& buf = LoadOggByPath(path);
    if (buf.alBuffer == 0) {
        ASCIIgL::Logger::Errorf("[SoundSystem] Skipping track, buffer is invalid: %s", path.c_str());
        return;
    }

    alGenSources(1, &m_musicSource);
    alSourcei (m_musicSource, AL_BUFFER,         static_cast<ALint>(buf.alBuffer));
    alSourcef (m_musicSource, AL_GAIN,            0.5f);  // music at 50% — adjust to taste
    alSourcei (m_musicSource, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(m_musicSource, AL_POSITION,        0.0f, 0.0f, 0.0f);
    alSourcePlay(m_musicSource);
}

// ============================================================================
// Source pool
// ============================================================================

ALuint SoundSystem::AcquireSource()
{
    for (ALuint source : m_sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED || state == AL_INITIAL)
            return source;
    }

    ALuint source;
    alGenSources(1, &source);
    m_sources.push_back(source);
    return source;
}

void SoundSystem::RecycleFinishedSources()
{
    // AcquireSource() reuses stopped sources automatically.
    // Reserved for future cleanup (voice limiting, buffer eviction, etc.)
}

// ============================================================================
// OpenAL lifecycle
// ============================================================================

void SoundSystem::InitOpenAL()
{
    m_device = alcOpenDevice(nullptr);
    if (!m_device)
        throw std::runtime_error("SoundSystem: failed to open OpenAL device");

    m_context = alcCreateContext(m_device, nullptr);
    if (!m_context)
        throw std::runtime_error("SoundSystem: failed to create OpenAL context");

    if (!alcMakeContextCurrent(m_context))
        throw std::runtime_error("SoundSystem: failed to make OpenAL context current");

    // Default listener — forward = -Z, up = +Y (matches OpenGL convention)
    ALfloat orientation[] = {
        0.0f, 0.0f, -1.0f,  // forward
        0.0f, 1.0f,  0.0f   // up
    };
    alListenerfv(AL_ORIENTATION, orientation);
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alListenerf (AL_GAIN,     1.0f);

    ASCIIgL::Logger::Info("[SoundSystem] OpenAL initialized");
}

void SoundSystem::ShutdownOpenAL()
{
    if (m_musicSource != 0) {
        alSourceStop(m_musicSource);
        alDeleteSources(1, &m_musicSource);
        m_musicSource = 0;
    }

    for (ALuint source : m_sources) {
        alSourceStop(source);
        alDeleteSources(1, &source);
    }
    m_sources.clear();

    for (auto& [id, buf] : m_buffers) {
        if (buf.alBuffer != 0)
            alDeleteBuffers(1, &buf.alBuffer);
    }
    m_buffers.clear();

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(m_context);
    alcCloseDevice(m_device);

    m_context = nullptr;
    m_device  = nullptr;

    ASCIIgL::Logger::Info("[SoundSystem] OpenAL shutdown");
}

} // namespace ecs::systems