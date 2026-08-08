#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include <ASCIICraft/world/Coords.hpp>

/// What a queued chunk job does. Carried alongside the task so a scheduler can
/// reason about pending work without executing it.
enum class ChunkJobKind : uint8_t {
    Terrain,
    Mesh,
    Unload,
};

/// Identifies a queued job. Tests select jobs by (kind, coord) to control the order
/// they run in; the production scheduler ignores it.
struct ChunkJobTag {
    ChunkJobKind kind;
    ChunkCoord   coord;
};

/// Execution strategy for chunk jobs.
///
/// Production uses oneTBB. Tests substitute a manual pump that holds jobs in a list
/// and runs them in a chosen order, which turns load/unload races into deterministic,
/// assertable sequences instead of timing-dependent flakes.
///
/// Implementations must not return from their destructor while a submitted task is
/// still running - tasks capture state owned by the ChunkJobQueue.
class IChunkJobScheduler {
public:
    virtual ~IChunkJobScheduler() = default;

    IChunkJobScheduler() = default;
    IChunkJobScheduler(const IChunkJobScheduler&) = delete;
    IChunkJobScheduler& operator=(const IChunkJobScheduler&) = delete;

    /// Submit a task. May run immediately, later, or never, depending on strategy.
    virtual void Run(ChunkJobTag tag, std::function<void()> task) = 0;

    /// Block until every submitted task has finished.
    virtual void Wait() = 0;
};

/// The production scheduler: a oneTBB task_group. Behaviour matches the direct
/// task_group usage this interface replaced.
std::unique_ptr<IChunkJobScheduler> MakeTbbChunkJobScheduler();
