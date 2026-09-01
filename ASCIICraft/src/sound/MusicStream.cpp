#include <ASCIICraft/sound/MusicStream.hpp>

#include <ASCIICraft/sound/StbVorbis.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <utility>

namespace sound {

MusicStream::~MusicStream()
{
    Stop();
}

bool MusicStream::Open(const std::string& path)
{
    // Idempotent reset, so reopening an active stream is one call.
    Stop();

    int error = 0;
    m_vorbis = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
    if (m_vorbis == nullptr) {
        ASCIIgL::Logger::Errorf(
            "[MusicStream] Failed to open ogg: %s (stb_vorbis error: %d)", path.c_str(), error);
        return false;
    }

    const stb_vorbis_info info = stb_vorbis_get_info(m_vorbis);
    if (info.channels <= 0 || info.sample_rate <= 0) {
        ASCIIgL::Logger::Errorf(
            "[MusicStream] Bad stream format for %s (channels: %d, rate: %d)",
            path.c_str(), info.channels, static_cast<int>(info.sample_rate));
        stb_vorbis_close(m_vorbis);
        m_vorbis = nullptr;
        return false;
    }

    m_channels   = info.channels;
    m_sampleRate = static_cast<int>(info.sample_rate);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = true;
    }

    // Started last: the worker reads m_vorbis, m_channels and m_open, all of
    // which are settled by this point.
    m_thread = std::thread(&MusicStream::DecodeLoop, this);
    return true;
}

void MusicStream::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = true;
    }
    m_cv.notify_all();

    if (m_thread.joinable()) {
        m_thread.join();
    }

    // Only safe once the worker is joined - it decodes straight from this handle.
    if (m_vorbis != nullptr) {
        stb_vorbis_close(m_vorbis);
        m_vorbis = nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_ready.clear();
    m_free.clear();
    m_open          = false;
    m_eof           = false;
    m_stopRequested = false;
    m_channels      = 0;
    m_sampleRate    = 0;
}

bool MusicStream::IsOpen() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_open;
}

int MusicStream::Channels() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_channels;
}

int MusicStream::SampleRate() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sampleRate;
}

bool MusicStream::PopChunk(Chunk& out)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_ready.empty()) {
        return false;
    }

    // After the swap the front holds the caller's spent buffer, which goes back
    // to the free list so steady-state playback allocates nothing.
    std::swap(out, m_ready.front());
    Chunk spent = std::move(m_ready.front());
    m_ready.pop_front();

    spent.frames = 0;
    if (m_free.size() < RING_CHUNKS) {
        m_free.push_back(std::move(spent));
    }

    m_cv.notify_one();
    return true;
}

bool MusicStream::IsExhausted() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_eof && m_ready.empty();
}

void MusicStream::DecodeLoop()
{
    const int channels = m_channels;

    for (;;) {
        Chunk chunk;
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Backpressure: park until playback drains a slot. This is why the
            // decode runs on its own thread rather than a TBB task - a pooled
            // worker parked here for the length of a track would starve chunk
            // terrain and mesh generation.
            m_cv.wait(lock, [this] {
                return m_stopRequested || m_ready.size() < RING_CHUNKS;
            });

            if (m_stopRequested) {
                return;
            }

            if (!m_free.empty()) {
                chunk = std::move(m_free.back());
                m_free.pop_back();
            }
        }

        chunk.pcm.resize(static_cast<size_t>(CHUNK_FRAMES) * static_cast<size_t>(channels));

        // Unlocked: the decode is the slow part, and the handle is untouched by
        // any other thread while the worker is alive.
        const int frames = stb_vorbis_get_samples_short_interleaved(
            m_vorbis, channels, chunk.pcm.data(), CHUNK_FRAMES * channels);

        chunk.frames = frames;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopRequested) {
            return;
        }

        if (frames <= 0) {
            // End of stream. Set under the lock together with the final queue
            // state so IsExhausted() can never observe "no chunks left" while a
            // decoded chunk is still in flight.
            m_eof = true;
            m_cv.notify_all();
            return;
        }

        m_ready.push_back(std::move(chunk));
    }
}

} // namespace sound
