#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward-declared so the vendored decoder stays out of the public include tree;
// only MusicStream.cpp pulls in <ASCIICraft/sound/StbVorbis.hpp>.
struct stb_vorbis;

namespace sound {

/// Incrementally decodes an ogg file on a dedicated worker thread.
///
/// Music tracks are 1.7-2.7 MB on disk but 33-50 MB once decoded, so decoding a
/// whole track up front - which is what SoundSystem used to do, on the game
/// thread, the frame the track was requested - stalled the frame badly. This
/// hands out small chunks instead, keeping roughly RING_CHUNKS of audio buffered
/// ahead of playback.
///
/// Deliberately free of any OpenAL dependency: the worker only produces PCM, and
/// every AL call stays on the main thread in SoundSystem. That also makes this
/// class unit-testable headless, which SoundSystem is not (its constructor opens
/// an audio device).
///
/// Thread safety: Open/Stop/PopChunk/IsExhausted are for the owning thread only.
/// The worker touches nothing else in the process.
class MusicStream {
public:
    /// Frames (samples per channel) decoded per chunk - about 93 ms at 44.1 kHz.
    static constexpr int CHUNK_FRAMES = 4096;

    /// Chunks buffered ahead of playback, ~0.75 s. The worker blocks once this
    /// many are ready, so a stream costs bounded memory no matter how long the
    /// track is.
    static constexpr size_t RING_CHUNKS = 8;

    /// Interleaved 16-bit PCM. \c frames is samples per channel, so the live
    /// portion of \c pcm is frames * channels shorts - it may be shorter than
    /// the vector, which is sized for a full chunk and reused.
    struct Chunk {
        std::vector<short> pcm;
        int                frames = 0;
    };

    MusicStream() = default;
    ~MusicStream();

    // Owns a thread and a mutex.
    MusicStream(const MusicStream&)            = delete;
    MusicStream& operator=(const MusicStream&) = delete;

    /// Opens \p path and starts decoding. Returns false if the file cannot be
    /// opened or reports nonsense format data; the stream is left closed.
    ///
    /// The header parse happens on the calling thread, so Channels() and
    /// SampleRate() are valid the moment this returns true. Safe to call on an
    /// already-open stream - the previous one is stopped first.
    bool Open(const std::string& path);

    /// Stops the worker, joins it, and releases the decoder. Idempotent, and
    /// safe on a stream that was never opened or already finished. Leaves the
    /// object reusable by a later Open().
    void Stop();

    bool IsOpen() const;

    /// Valid only while open; 0 otherwise.
    int Channels() const;
    int SampleRate() const;

    /// Moves one decoded chunk into \p out and recycles \p out's previous buffer
    /// for the worker. False if none is ready yet, which is not an error - it
    /// means playback has caught up with the decoder.
    bool PopChunk(Chunk& out);

    /// True once the decoder has reached end of file AND every chunk has been
    /// popped. Only meaningful while open.
    bool IsExhausted() const;

private:
    void DecodeLoop();

    stb_vorbis* m_vorbis = nullptr;
    std::thread m_thread;

    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;

    std::deque<Chunk>  m_ready;  ///< decoded, waiting to be popped
    std::vector<Chunk> m_free;   ///< spent buffers, recycled to avoid steady-state allocation

    bool m_open          = false;
    bool m_eof           = false;
    bool m_stopRequested = false;

    // Written once in Open() before the worker starts, read-only thereafter.
    int m_channels   = 0;
    int m_sampleRate = 0;
};

} // namespace sound
