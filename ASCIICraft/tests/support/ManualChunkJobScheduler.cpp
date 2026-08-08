#include "support/ManualChunkJobScheduler.hpp"

#include <algorithm>
#include <utility>

namespace testsupport {
namespace {

size_t KindIndex(ChunkJobKind kind) { return static_cast<size_t>(kind); }

/// splitmix64 - deterministic and self-contained, so a recorded seed reproduces a
/// failing schedule exactly on any machine.
uint64_t NextRandom(uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

} // namespace

void ManualChunkJobScheduler::SetPolicy(ChunkJobKind kind, Policy policy) {
    policies_[KindIndex(kind)] = policy;
}

ManualChunkJobScheduler::Policy ManualChunkJobScheduler::GetPolicy(ChunkJobKind kind) const {
    return policies_[KindIndex(kind)];
}

ManualChunkJobScheduler::Policy ManualChunkJobScheduler::PolicyFor(ChunkJobKind kind) const {
    return policies_[KindIndex(kind)];
}

void ManualChunkJobScheduler::Run(ChunkJobTag tag, std::function<void()> task) {
    switch (PolicyFor(tag.kind)) {
        case Policy::Drop:
            return;
        case Policy::RunInline:
            ++executed_[KindIndex(tag.kind)];
            task();
            return;
        case Policy::Queue:
            pending_.push_back(Job{ tag, std::move(task) });
            return;
    }
}

void ManualChunkJobScheduler::Wait() {
    // The production contract is "block until everything finishes". With no threads
    // the equivalent is to run everything now, which is also what ChunkManager::SaveAll
    // needs in order to see drained results.
    RunAll();
}

size_t ManualChunkJobScheduler::PendingCount() const {
    return pending_.size();
}

size_t ManualChunkJobScheduler::CountPending(ChunkJobKind kind) const {
    return static_cast<size_t>(std::count_if(pending_.begin(), pending_.end(),
        [kind](const Job& j) { return j.tag.kind == kind; }));
}

size_t ManualChunkJobScheduler::CountPending(ChunkJobKind kind, ChunkCoord coord) const {
    return static_cast<size_t>(std::count_if(pending_.begin(), pending_.end(),
        [kind, coord](const Job& j) { return j.tag.kind == kind && j.tag.coord == coord; }));
}

std::vector<ChunkJobTag> ManualChunkJobScheduler::PendingTags() const {
    std::vector<ChunkJobTag> tags;
    tags.reserve(pending_.size());
    for (const Job& j : pending_) tags.push_back(j.tag);
    return tags;
}

void ManualChunkJobScheduler::RunAll(size_t maxJobs) {
    if (running_) return;   // a job called Wait(); let the outer loop drain it
    running_ = true;

    size_t executed = 0;
    while (!pending_.empty() && executed < maxJobs) {
        Job job = std::move(pending_.front());
        pending_.pop_front();
        ++executed_[KindIndex(job.tag.kind)];
        ++executed;
        job.task();
    }

    running_ = false;
}

void ManualChunkJobScheduler::RunAllOfKind(ChunkJobKind kind) {
    while (RunFirstOfKind(kind)) {}
}

bool ManualChunkJobScheduler::RunFirst(ChunkJobKind kind, ChunkCoord coord) {
    auto it = std::find_if(pending_.begin(), pending_.end(),
        [kind, coord](const Job& j) { return j.tag.kind == kind && j.tag.coord == coord; });
    if (it == pending_.end()) return false;

    Job job = std::move(*it);
    pending_.erase(it);
    ++executed_[KindIndex(kind)];
    job.task();
    return true;
}

bool ManualChunkJobScheduler::RunFirstOfKind(ChunkJobKind kind) {
    auto it = std::find_if(pending_.begin(), pending_.end(),
        [kind](const Job& j) { return j.tag.kind == kind; });
    if (it == pending_.end()) return false;

    Job job = std::move(*it);
    pending_.erase(it);
    ++executed_[KindIndex(kind)];
    job.task();
    return true;
}

bool ManualChunkJobScheduler::DropFirst(ChunkJobKind kind, ChunkCoord coord) {
    auto it = std::find_if(pending_.begin(), pending_.end(),
        [kind, coord](const Job& j) { return j.tag.kind == kind && j.tag.coord == coord; });
    if (it == pending_.end()) return false;
    pending_.erase(it);
    return true;
}

size_t ManualChunkJobScheduler::DropAllOfKind(ChunkJobKind kind) {
    const size_t before = pending_.size();
    pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
        [kind](const Job& j) { return j.tag.kind == kind; }), pending_.end());
    return before - pending_.size();
}

void ManualChunkJobScheduler::RunRandomSubset(uint64_t& rngState, double fraction) {
    // Snapshot the count first: jobs run here may enqueue more, and those should wait
    // for the next call rather than cascading within this one.
    const size_t target = static_cast<size_t>(static_cast<double>(pending_.size()) * fraction);
    for (size_t i = 0; i < target && !pending_.empty(); ++i) {
        const size_t index = static_cast<size_t>(NextRandom(rngState) % pending_.size());
        auto it = pending_.begin() + static_cast<std::ptrdiff_t>(index);
        Job job = std::move(*it);
        pending_.erase(it);
        ++executed_[KindIndex(job.tag.kind)];
        job.task();
    }
}

void ManualChunkJobScheduler::RunRandomSubsetOfKind(ChunkJobKind kind,
                                                    uint64_t& rngState,
                                                    double fraction) {
    const size_t available = CountPending(kind);
    const size_t target = static_cast<size_t>(static_cast<double>(available) * fraction);

    for (size_t i = 0; i < target; ++i) {
        // Collect current positions of this kind, then pick one. Recomputed each
        // iteration because a running job may enqueue or invalidate others.
        std::vector<size_t> indices;
        for (size_t j = 0; j < pending_.size(); ++j) {
            if (pending_[j].tag.kind == kind) indices.push_back(j);
        }
        if (indices.empty()) return;

        const size_t pick = indices[static_cast<size_t>(NextRandom(rngState) % indices.size())];
        auto it = pending_.begin() + static_cast<std::ptrdiff_t>(pick);
        Job job = std::move(*it);
        pending_.erase(it);
        ++executed_[KindIndex(kind)];
        job.task();
    }
}

size_t ManualChunkJobScheduler::ExecutedCount(ChunkJobKind kind) const {
    return executed_[KindIndex(kind)];
}

} // namespace testsupport
