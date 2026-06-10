#include <ASCIICraft/ecs/systems/sound/SoundSystem.hpp>

#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/sound/SoundRegistry.hpp>

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include <ASCIIgL/util/Logger.hpp>

#include <stdexcept>
#include <string>

namespace ecs::systems {

namespace {

/// Interpolated body origin (feet); matches what the camera uses for horizontal motion.
glm::vec3 GetAudioWorldPosition(const components::Transform& transform) {
    return transform.renderPosition;
}

/// Footsteps and similar SFX emit at the collider bottom in world space.
glm::vec3 GetFootstepEmitPosition(entt::registry& registry, entt::entity ent) {
    const auto& transform = registry.get<components::Transform>(ent);
    glm::vec3 pos = transform.renderPosition;

    if (const auto* col = registry.try_get<components::Collider>(ent)) {
        pos = transform.renderPosition + col->localOffset;
        pos.y -= col->halfExtents.y;
    }

    return pos;
}

bool IsStepSoundId(const std::string& soundId) {
    return soundId.size() >= 5 && soundId.compare(0, 5, "step.") == 0;
}

} // namespace

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

bool SoundSystem::IsMusicPlaying() const
{
    if (m_musicSource == 0) {
        return false;
    }

    ALint state = AL_STOPPED;
    alGetSourcei(m_musicSource, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

void SoundSystem::Update()
{
    RecycleFinishedSources();

    const entt::entity player = components::GetPlayerEntity(m_registry);
    if (player != entt::null && m_registry.all_of<components::PlayerCamera>(player)) {
        const ASCIIgL::Camera3D& cam = m_registry.get<components::PlayerCamera>(player).camera;
        const glm::vec3 at = cam.getCamFront();
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const ALfloat orientation[] = { at.x, at.y, at.z, up.x, up.y, up.z };
        alListenerfv(AL_ORIENTATION, orientation);
        alListener3f(AL_POSITION, cam.pos.x, cam.pos.y, cam.pos.z);
    }

    for (const auto& event : m_eventBus.view<events::PlayMusicEvent>()) {
        OnPlayMusic(event);
    }

    for (const auto& event : m_eventBus.view<events::PlaySoundEvent>()) {
        OnPlaySound(event);
    }
}

void SoundSystem::OnPlaySound(const events::PlaySoundEvent& event)
{
    SoundBuffer& buf = LoadSoundId(event.soundId);
    if (buf.alBuffer == 0) {
        return;
    }

    const ALuint source = AcquireSource();
    if (source == 0) {
        return;
    }

    alSourcei(source, AL_BUFFER, static_cast<ALint>(buf.alBuffer));
    alSourcef(source, AL_GAIN, event.volume);
    alSourcef(source, AL_PITCH, event.pitch);

    const bool localPlayerStep = event.entity != entt::null
        && m_registry.valid(event.entity)
        && IsStepSoundId(event.soundId)
        && m_registry.all_of<components::PlayerTag>(event.entity);

    if (localPlayerStep) {
        // Own footsteps: non-positional so they don't trail behind as you move.
        alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    } else if (event.entity != entt::null && m_registry.valid(event.entity)
               && m_registry.all_of<components::Transform>(event.entity)) {
        const glm::vec3 emitPos = IsStepSoundId(event.soundId)
            ? GetFootstepEmitPosition(m_registry, event.entity)
            : GetAudioWorldPosition(m_registry.get<components::Transform>(event.entity));
        alSource3f(source, AL_POSITION, emitPos.x, emitPos.y, emitPos.z);
        alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    } else {
        alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    }

    alSourcePlay(source);
}

void SoundSystem::OnPlayMusic(const events::PlayMusicEvent& event)
{
    if (m_musicSource != 0) {
        ALint state = AL_STOPPED;
        alGetSourcei(m_musicSource, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) {
            return;
        }

        alDeleteSources(1, &m_musicSource);
        m_musicSource = 0;
    }

    SoundBuffer& buf = LoadSoundId(event.soundId);
    if (buf.alBuffer == 0) {
        ASCIIgL::Logger::Errorf("[SoundSystem] Skipping track, buffer is invalid: %s", event.soundId.c_str());
        return;
    }

    alGenSources(1, &m_musicSource);
    alSourcei(m_musicSource, AL_BUFFER, static_cast<ALint>(buf.alBuffer));
    alSourcef(m_musicSource, AL_GAIN, event.volume);
    alSourcei(m_musicSource, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(m_musicSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSourcePlay(m_musicSource);

    ASCIIgL::Logger::Infof("[SoundSystem] Playing music track: %s", event.soundId.c_str());
}

SoundSystem::SoundBuffer& SoundSystem::LoadSoundId(const std::string& soundId)
{
    const auto* soundRegistry = m_registry.ctx().find<sound::SoundRegistry>();
    if (!soundRegistry) {
        ASCIIgL::Logger::Error("[SoundSystem] SoundRegistry missing from registry context");
        static SoundBuffer empty;
        return empty;
    }

    if (!soundRegistry->Has(soundId)) {
        ASCIIgL::Logger::Errorf("[SoundSystem] Unknown sound id: %s", soundId.c_str());
        static SoundBuffer empty;
        return empty;
    }

    const std::string path = soundRegistry->PickRandomPath(soundId);
    if (path.empty()) {
        ASCIIgL::Logger::Errorf("[SoundSystem] No paths for sound id: %s", soundId.c_str());
        static SoundBuffer empty;
        return empty;
    }

    return LoadOggByPath(path);
}

SoundSystem::SoundBuffer& SoundSystem::LoadOggByPath(const std::string& path)
{
    auto it = m_buffers.find(path);
    if (it != m_buffers.end()) {
        return it->second;
    }

    ASCIIgL::Logger::Infof("[SoundSystem] Loading ogg: %s", path.c_str());

    SoundBuffer buf = DecodeOgg(path);
    if (buf.alBuffer == 0) {
        ASCIIgL::Logger::Errorf("[SoundSystem] Failed to load ogg from path: %s", path.c_str());
    }

    auto [inserted, _] = m_buffers.emplace(path, buf);
    return inserted->second;
}

SoundSystem::SoundBuffer SoundSystem::DecodeOgg(const std::string& path)
{
    SoundBuffer result;

    int channels = 0;
    int sampleRate = 0;
    short* pcmData = nullptr;

    const int sampleCount = stb_vorbis_decode_filename(path.c_str(), &channels, &sampleRate, &pcmData);

    if (sampleCount < 0 || pcmData == nullptr) {
        ASCIIgL::Logger::Errorf(
            "[SoundSystem] stb_vorbis failed to decode: %s (error code: %d)",
            path.c_str(),
            sampleCount
        );
        return result;
    }

    const ALenum format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    alGenBuffers(1, &result.alBuffer);
    alBufferData(
        result.alBuffer,
        format,
        pcmData,
        sampleCount * channels * static_cast<int>(sizeof(short)),
        sampleRate
    );

    const ALenum alErr = alGetError();
    if (alErr != AL_NO_ERROR) {
        ASCIIgL::Logger::Errorf("[SoundSystem] alBufferData failed for %s (AL error: %d)", path.c_str(), alErr);
        alDeleteBuffers(1, &result.alBuffer);
        result.alBuffer = 0;
    }

    result.channels = channels;
    result.sampleRate = sampleRate;

    free(pcmData);
    return result;
}

ALuint SoundSystem::AcquireSource()
{
    for (ALuint source : m_sources) {
        ALint state = AL_STOPPED;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED || state == AL_INITIAL) {
            return source;
        }
    }

    if (static_cast<int>(m_sources.size()) < MAX_SFX_VOICES) {
        ALuint source = 0;
        alGenSources(1, &source);
        m_sources.push_back(source);
        return source;
    }

    // At voice cap — stop the oldest pooled source (first currently playing).
    for (ALuint source : m_sources) {
        ALint state = AL_STOPPED;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) {
            alSourceStop(source);
            return source;
        }
    }

    return 0;
}

void SoundSystem::RecycleFinishedSources()
{
    // Voices are reused via AcquireSource(); no eviction needed yet.
}

void SoundSystem::InitOpenAL()
{
    m_device = alcOpenDevice(nullptr);
    if (!m_device) {
        throw std::runtime_error("SoundSystem: failed to open OpenAL device");
    }

    m_context = alcCreateContext(m_device, nullptr);
    if (!m_context) {
        throw std::runtime_error("SoundSystem: failed to create OpenAL context");
    }

    if (!alcMakeContextCurrent(m_context)) {
        throw std::runtime_error("SoundSystem: failed to make OpenAL context current");
    }

    const ALfloat orientation[] = {
        0.0f, 0.0f, -1.0f,
        0.0f, 1.0f,  0.0f,
    };
    alListenerfv(AL_ORIENTATION, orientation);
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alListenerf(AL_GAIN, 1.0f);

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
        if (buf.alBuffer != 0) {
            alDeleteBuffers(1, &buf.alBuffer);
        }
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
