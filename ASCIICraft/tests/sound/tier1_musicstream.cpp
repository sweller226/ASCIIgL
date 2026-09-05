// Incremental ogg decoding for music streaming.
//
// MusicStream exists because decoding a whole track up front stalled the frame it
// was requested on: 1.7-2.7 MB on disk becomes 33-50 MB of PCM. Handing out small
// chunks off a worker thread fixes that, but only if the chunks reassemble into
// exactly the same audio - a chunk-boundary or interleaving bug would be audible
// as a click or a channel swap, and would not show up as a crash or a leak.
//
// So the load-bearing test here compares a full drain against
// stb_vorbis_decode_filename on the same file, sample for sample. The rest pin the
// lifecycle: reopening, stopping mid-decode, and stopping something never opened.
//
// MusicStream is deliberately OpenAL-free, which is what lets these run headless -
// SoundSystem itself cannot be constructed in a test because it opens an audio
// device.

#include <doctest/doctest.h>

#include <ASCIICraft/sound/MusicStream.hpp>
#include <ASCIICraft/sound/StbVorbis.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

// Resolved against the test exe's directory, which CTest sets as the working
// directory and where res/ is staged.
constexpr const char* kSmallOgg = "res/sounds/dig/cloth1.ogg";   // ~4.8 KB
constexpr const char* kOtherOgg = "res/sounds/dig/coral1.ogg";   // ~15.9 KB
constexpr const char* kLargeOgg = "res/sounds/music/game/piano3.ogg"; // ~2.6 MB
constexpr const char* kMissingOgg = "res/sounds/definitely_not_here.ogg";

struct ReferenceDecode {
    int                channels   = 0;
    int                sampleRate = 0;
    std::vector<short> pcm; ///< interleaved
};

/// Ground truth: the whole-file decode MusicStream is meant to reproduce.
ReferenceDecode DecodeWhole(const char* path) {
    ReferenceDecode ref;
    short*          raw    = nullptr;
    const int       frames = stb_vorbis_decode_filename(path, &ref.channels, &ref.sampleRate, &raw);

    if (frames > 0 && raw != nullptr) {
        ref.pcm.assign(raw, raw + static_cast<size_t>(frames) * static_cast<size_t>(ref.channels));
    }
    free(raw);
    return ref;
}

/// Drains to exhaustion and returns the interleaved PCM. Fails the test rather
/// than hanging if the decoder ever stops producing.
std::vector<short> DrainAll(sound::MusicStream& stream) {
    const int channels = stream.Channels();

    std::vector<short>        out;
    sound::MusicStream::Chunk chunk;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

    while (!stream.IsExhausted()) {
        if (stream.PopChunk(chunk)) {
            const auto used = static_cast<size_t>(chunk.frames) * static_cast<size_t>(channels);
            out.insert(out.end(), chunk.pcm.begin(), chunk.pcm.begin() + used);
        } else if (std::chrono::steady_clock::now() > deadline) {
            FAIL("MusicStream stalled: no chunk produced before the deadline");
            break;
        } else {
            std::this_thread::yield();
        }
    }

    return out;
}

} // namespace

TEST_SUITE("sound.tier1.musicstream") {

TEST_CASE("opening a missing file fails and leaves the stream closed") {
    sound::MusicStream stream;

    CHECK_FALSE(stream.Open(kMissingOgg));
    CHECK_FALSE(stream.IsOpen());
    CHECK(stream.Channels() == 0);
    CHECK(stream.SampleRate() == 0);
}

TEST_CASE("format is read from the header before Open returns") {
    // SoundSystem picks its AL_FORMAT from these the moment Open succeeds, so
    // they must be valid immediately rather than once the worker gets going.
    const ReferenceDecode ref = DecodeWhole(kSmallOgg);
    REQUIRE(ref.channels > 0);

    sound::MusicStream stream;
    REQUIRE(stream.Open(kSmallOgg));

    CHECK(stream.IsOpen());
    CHECK(stream.Channels() == ref.channels);
    CHECK(stream.SampleRate() == ref.sampleRate);
}

TEST_CASE("streamed output is identical to a whole-file decode") {
    // The reason this class exists at all. A chunk-boundary or interleaving bug
    // would be audible but otherwise silent - nothing else here would catch it.
    const ReferenceDecode ref = DecodeWhole(kSmallOgg);
    REQUIRE_FALSE(ref.pcm.empty());

    sound::MusicStream stream;
    REQUIRE(stream.Open(kSmallOgg));

    const std::vector<short> got = DrainAll(stream);

    REQUIRE(got.size() == ref.pcm.size());
    CHECK(got == ref.pcm);
}

TEST_CASE("chunks stay within the declared frame budget") {
    sound::MusicStream stream;
    REQUIRE(stream.Open(kSmallOgg));

    sound::MusicStream::Chunk chunk;
    int  chunksSeen = 0;
    bool oversized  = false;
    bool empty      = false;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!stream.IsExhausted()) {
        if (stream.PopChunk(chunk)) {
            ++chunksSeen;
            if (chunk.frames > sound::MusicStream::CHUNK_FRAMES) oversized = true;
            if (chunk.frames <= 0) empty = true;
        } else if (std::chrono::steady_clock::now() > deadline) {
            FAIL("MusicStream stalled");
            break;
        } else {
            std::this_thread::yield();
        }
    }

    CHECK(chunksSeen > 0);
    CHECK_FALSE(oversized); // a larger chunk would overrun the AL buffer upload
    CHECK_FALSE(empty);     // an empty chunk means EOF and must never be queued
}

TEST_CASE("an exhausted stream reports so and yields nothing further") {
    sound::MusicStream stream;
    REQUIRE(stream.Open(kSmallOgg));

    const std::vector<short> got = DrainAll(stream);
    REQUIRE_FALSE(got.empty());

    CHECK(stream.IsExhausted());

    // PumpMusicStream relies on this to decide a track has genuinely ended
    // rather than merely underrun.
    sound::MusicStream::Chunk chunk;
    CHECK_FALSE(stream.PopChunk(chunk));
}

TEST_CASE("Stop is idempotent and safe on an unopened stream") {
    sound::MusicStream stream;

    stream.Stop(); // never opened
    CHECK_FALSE(stream.IsOpen());

    REQUIRE(stream.Open(kSmallOgg));
    DrainAll(stream);

    stream.Stop();
    stream.Stop(); // idempotent
    CHECK_FALSE(stream.IsOpen());
    CHECK(stream.Channels() == 0);
}

TEST_CASE("a stream can be reopened for a different file") {
    // SoundSystem reuses one MusicStream for every track, so Open must reset
    // whatever the previous track left behind.
    const ReferenceDecode second = DecodeWhole(kOtherOgg);
    REQUIRE_FALSE(second.pcm.empty());

    sound::MusicStream stream;

    REQUIRE(stream.Open(kSmallOgg));
    DrainAll(stream);

    REQUIRE(stream.Open(kOtherOgg));
    CHECK(stream.Channels() == second.channels);

    const std::vector<short> got = DrainAll(stream);
    REQUIRE(got.size() == second.pcm.size());
    CHECK(got == second.pcm);
}

TEST_CASE("Open on a still-running stream replaces it cleanly") {
    // Reopening without draining first is the interesting case: the worker is
    // mid-decode and blocked on backpressure when Open calls Stop underneath it.
    sound::MusicStream stream;

    REQUIRE(stream.Open(kLargeOgg));

    sound::MusicStream::Chunk chunk;
    for (int i = 0; i < 3; ++i) {
        stream.PopChunk(chunk);
    }

    REQUIRE(stream.Open(kSmallOgg));

    const ReferenceDecode ref = DecodeWhole(kSmallOgg);
    const std::vector<short> got = DrainAll(stream);
    REQUIRE(got.size() == ref.pcm.size());
    CHECK(got == ref.pcm);
}

TEST_CASE("Stop mid-decode joins without hanging") {
    // A long track guarantees the worker is still decoding, so this exercises the
    // stop-while-blocked path rather than a worker that has already finished.
    sound::MusicStream stream;
    REQUIRE(stream.Open(kLargeOgg));

    sound::MusicStream::Chunk chunk;
    for (int i = 0; i < 3; ++i) {
        stream.PopChunk(chunk);
    }

    stream.Stop();
    CHECK_FALSE(stream.IsOpen());
}

TEST_CASE("destruction while decoding does not leak the worker") {
    // The destructor is the path SoundSystem relies on at shutdown. If it failed
    // to join, this would crash or hang rather than fail cleanly.
    sound::MusicStream stream;
    REQUIRE(stream.Open(kLargeOgg));

    sound::MusicStream::Chunk chunk;
    stream.PopChunk(chunk);
    // Falls out of scope still decoding.
}

} // TEST_SUITE("sound.tier1.musicstream")
