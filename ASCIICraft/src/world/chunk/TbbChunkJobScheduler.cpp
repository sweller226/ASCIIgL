#include <ASCIICraft/world/chunk/IChunkJobScheduler.hpp>

#include <utility>

#include <oneapi/tbb/task_group.h>

namespace {

/// oneTBB-backed scheduler. Wraps a single task_group exactly as ChunkJobQueue used
/// to hold one directly, so production scheduling behaviour is unchanged: no job
/// ordering, no prioritisation, no cancellation.
class TbbChunkJobScheduler final : public IChunkJobScheduler {
public:
    // Explicitly noexcept: tbb::task_group's destructor is noexcept(false) (it throws
    // missing_wait when work is still pending), which would otherwise give this
    // destructor a weaker exception specification than the base's implicit noexcept.
    ~TbbChunkJobScheduler() noexcept override {
        // Must drain before taskGroup_ is destroyed. Tasks capture state owned by
        // ChunkJobQueue, so returning here with work in flight is a use-after-free.
        // This mirrors what ~ChunkJobQueue did when it owned the task_group itself.
        // Waiting here is also what stops task_group's own destructor from throwing.
        try {
            taskGroup_.wait();
        } catch (...) {
            // A task escaped an exception. Swallow it: propagating out of a
            // destructor terminates, and there is nothing useful to do at teardown.
        }
    }

    void Run(ChunkJobTag /*tag*/, std::function<void()> task) override {
        taskGroup_.run(std::move(task));
    }

    void Wait() override {
        taskGroup_.wait();
    }

private:
    oneapi::tbb::task_group taskGroup_;
};

} // namespace

std::unique_ptr<IChunkJobScheduler> MakeTbbChunkJobScheduler() {
    return std::make_unique<TbbChunkJobScheduler>();
}
