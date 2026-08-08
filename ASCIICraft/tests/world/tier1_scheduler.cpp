// IChunkJobScheduler contract tests.
//
// The oneTBB scheduler replaced a task_group that ChunkJobQueue owned inline. The
// property that made that safe - nothing returns while a submitted task is still
// running - is now the scheduler's responsibility, and a hard-killed game process
// never exercises it. These tests pin it directly.

#include <doctest/doctest.h>

#include <ASCIICraft/world/chunk/IChunkJobScheduler.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {

// Not constexpr: ChunkCoord's constructors are not constexpr, so it is not a
// literal type. The tag is irrelevant to the TBB scheduler, which ignores it.
const ChunkJobTag kAnyTag{ ChunkJobKind::Terrain, ChunkCoord{0, 0, 0} };

/// Burns a few milliseconds so tasks are genuinely still in flight when the
/// scheduler is torn down, rather than having already completed by luck.
void SlowWork(std::atomic<int>& counter) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

TEST_SUITE("world.tier1.scheduler") {

TEST_CASE("submitted tasks run") {
    auto scheduler = MakeTbbChunkJobScheduler();
    std::atomic<int> ran{0};

    for (int i = 0; i < 16; ++i) {
        scheduler->Run(kAnyTag, [&ran]() { ran.fetch_add(1, std::memory_order_relaxed); });
    }
    scheduler->Wait();

    CHECK(ran.load() == 16);
}

TEST_CASE("Wait blocks until every task has finished") {
    auto scheduler = MakeTbbChunkJobScheduler();
    std::atomic<int> ran{0};

    for (int i = 0; i < 32; ++i) {
        scheduler->Run(kAnyTag, [&ran]() { SlowWork(ran); });
    }
    scheduler->Wait();

    // If Wait returned early this reads short - it must not merely "usually" pass.
    CHECK(ran.load() == 32);
}

TEST_CASE("destructor drains in-flight tasks") {
    // The critical property. ChunkJobQueue's tasks capture `this` and push into
    // members that are destroyed right after the scheduler; if the destructor
    // returned with work outstanding, those tasks would write to freed memory.
    std::atomic<int> ran{0};
    {
        auto scheduler = MakeTbbChunkJobScheduler();
        for (int i = 0; i < 32; ++i) {
            scheduler->Run(kAnyTag, [&ran]() { SlowWork(ran); });
        }
        // No explicit Wait - the destructor must do it.
    }
    CHECK(ran.load() == 32);
}

TEST_CASE("destructor does not propagate an exception from a task") {
    // Destructors are noexcept; an escaping task exception must be swallowed rather
    // than terminating the process at teardown.
    std::atomic<int> ran{0};
    {
        auto scheduler = MakeTbbChunkJobScheduler();
        scheduler->Run(kAnyTag, [&ran]() {
            ran.fetch_add(1, std::memory_order_relaxed);
            throw std::runtime_error("task failure");
        });
    }
    CHECK(ran.load() == 1);
}

TEST_CASE("Wait on an idle scheduler is a no-op") {
    auto scheduler = MakeTbbChunkJobScheduler();
    scheduler->Wait();
    scheduler->Wait();
    CHECK(true); // reaching here without hanging or throwing is the assertion
}

} // TEST_SUITE("world.tier1.scheduler")
