#include <ASCIICraft/ecs/systems/sound/SoundSystem.hpp>

#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/sound/SoundRegistry.hpp>

#include <ASCIICraft/sound/StbVorbis.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/Profiler.hpp>

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

bool SoundSystem::IsMusicActive() const
{
    // Still decoding, or still holding undrained buffers. Checking the source
    // state instead would report "finished" on the first underrun.
    if (m_musicStream.IsOpen()) {
        return true;
    }

    if (m_musicSource == 0) {
        return false;
    }

    ALint queued = 0;
    alGetSourcei(m_musicSource, AL_BUFFERS_QUEUED, &queued);
    return queued > 0;
}

void SoundSystem::Update()
{
    PROFILE_SCOPE("Sound.Update");

    PumpMusicStream();

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
    if (m_musicSource == 0) {
        return;
    }

    // A track already running wins - MusicSystem only dispatches once the
    // previous one has finished, so this means two requests landed at once.
    if (IsMusicActive()) {
        return;
    }

    const std::string path = ResolveSoundPath(event.soundId);
    if (path.empty()) {
        return;
    }

    // Detach the whole queue so every ring buffer is free for the new track.
    // Setting AL_BUFFER to 0 on a stopped source clears the processed and the
    // still-pending entries in one call.
    alSourceStop(m_musicSource);
    alSourcei(m_musicSource, AL_BUFFER, 0);
    m_freeMusicBuffers.assign(m_musicBuffers.begin(), m_musicBuffers.end());

    if (!m_musicStream.Open(path)) {
        ASCIIgL::Logger::Errorf("[SoundSystem] Skipping track, stream failed to open: %s",
                                event.soundId.c_str());
        return;
    }

    alSourcef(m_musicSource, AL_GAIN, event.volume);
    alSourcei(m_musicSource, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(m_musicSource, AL_POSITION, 0.0f, 0.0f, 0.0f);

    // No alSourcePlay here - nothing is queued yet. PumpMusicStream starts
    // playback as soon as the decoder hands over the first chunk.
    ASCIIgL::Logger::Infof("[SoundSystem] Streaming music track: %s", event.soundId.c_str());
}

void SoundSystem::PumpMusicStream()
{
    PROFILE_SCOPE("Sound.PumpMusicStream");

    if (m_musicSource == 0 || !m_musicStream.IsOpen()) {
        return;
    }

    // 1. Reclaim buffers the source has finished with.
    ALint processed = 0;
    alGetSourcei(m_musicSource, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(m_musicSource, 1, &buffer);
        if (buffer != 0) {
            m_freeMusicBuffers.push_back(buffer);
        }
    }

    // 2. Refill from the decoder. Uploading a chunk is a ~35 KB copy, versus the
    //    33-50 MB copy the old whole-track path did in a single frame.
    const int    channels = m_musicStream.Channels();
    const int    rate     = m_musicStream.SampleRate();
    const ALenum format   = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    while (!m_freeMusicBuffers.empty() && m_musicStream.PopChunk(m_musicChunk)) {
        const ALuint buffer = m_freeMusicBuffers.back();
        m_freeMusicBuffers.pop_back();

        alBufferData(
            buffer,
            format,
            m_musicChunk.pcm.data(),
            static_cast<ALsizei>(m_musicChunk.frames * channels * static_cast<int>(sizeof(short))),
            rate
        );
        alSourceQueueBuffers(m_musicSource, 1, &buffer);
    }

    ALint queued = 0;
    alGetSourcei(m_musicSource, AL_BUFFERS_QUEUED, &queued);

    // 3. The track is over only when the decoder is done AND the queue has drained.
    if (m_musicStream.IsExhausted() && queued == 0) {
        m_musicStream.Stop();
        ASCIIgL::Logger::Info("[SoundSystem] Music track finished");
        return;
    }

    // 4. Start playback, and recover if the decoder ever fell behind and the
    //    source ran dry mid-track.
    ALint state = AL_STOPPED;
    alGetSourcei(m_musicSource, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING && queued > 0) {
        alSourcePlay(m_musicSource);
    }
}

std::string SoundSystem::ResolveSoundPath(const std::string& soundId)
{
    const auto* soundRegistry = m_registry.ctx().find<sound::SoundRegistry>();
    if (!soundRegistry) {
        ASCIIgL::Logger::Error("[SoundSystem] SoundRegistry missing from registry context");
        return {};
    }

    if (!soundRegistry->Has(soundId)) {
        ASCIIgL::Logger::Errorf("[SoundSystem] Unknown sound id: %s", soundId.c_str());
        return {};
    }

    const std::string path = soundRegistry->PickRandomPath(soundId);
    if (path.empty()) {
        ASCIIgL::Logger::Errorf("[SoundSystem] No paths for sound id: %s", soundId.c_str());
    }

    return path;
}

SoundSystem::SoundBuffer& SoundSystem::LoadSoundId(const std::string& soundId)
{
    const std::string path = ResolveSoundPath(soundId);
    if (path.empty()) {
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

    const SoundBuffer buf = DecodeOgg(path);
    if (buf.alBuffer == 0) {
        // Deliberately not cached. Caching a failure would make a transient
        // problem permanent - every later request for this path would return the
        // dud entry without ever retrying the decode.
        ASCIIgL::Logger::Errorf("[SoundSystem] Failed to load ogg from path: %s", path.c_str());
        static SoundBuffer empty;
        return empty;
    }

    auto [inserted, _] = m_buffers.emplace(path, buf);
    return inserted->second;
}

SoundSystem::SoundBuffer SoundSystem::DecodeOgg(const std::string& path)
{
    // Whole-file decode, now used only by SFX (4-16 KB each). Music streams
    // instead - see PumpMusicStream. Worth keeping instrumented so a large file
    // sneaking onto this path shows up immediately.
    PROFILE_SCOPE("Sound.DecodeOgg");

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

    // Clear any error left behind by earlier AL calls. alGetError() returns the
    // oldest error since it was last read, so without this a stale error from,
    // say, a source operation would be attributed to the upload below and throw
    // away a perfectly good buffer.
    while (alGetError() != AL_NO_ERROR) {
    }

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

    // The music source and its ring of stream buffers live for the whole run.
    // Recreating them per track was pointless churn, and the ring has to outlive
    // any single track anyway since buffers are recycled as they drain.
    alGenSources(1, &m_musicSource);
    alSourcei(m_musicSource, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(m_musicSource, AL_POSITION, 0.0f, 0.0f, 0.0f);

    alGenBuffers(static_cast<ALsizei>(m_musicBuffers.size()), m_musicBuffers.data());
    m_freeMusicBuffers.assign(m_musicBuffers.begin(), m_musicBuffers.end());

    ASCIIgL::Logger::Info("[SoundSystem] OpenAL initialized");
}

void SoundSystem::ShutdownOpenAL()
{
    // Join the decoder before touching the source it feeds.
    m_musicStream.Stop();

    if (m_musicSource != 0) {
        alSourceStop(m_musicSource);
        alSourcei(m_musicSource, AL_BUFFER, 0);
        alDeleteSources(1, &m_musicSource);
        m_musicSource = 0;
    }

    alDeleteBuffers(static_cast<ALsizei>(m_musicBuffers.size()), m_musicBuffers.data());
    m_musicBuffers.fill(0);
    m_freeMusicBuffers.clear();

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
