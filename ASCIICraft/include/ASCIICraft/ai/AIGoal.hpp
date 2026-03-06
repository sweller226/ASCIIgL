#pragma once
// =========================================================================
// AIGoal.hpp — Goal-based AI scheduling (Minecraft EntityAITasks port)
//
// Provides:
//   AIGoal         — abstract base class for individual behaviors
//   AITaskScheduler — priority-based scheduler with mutex-bit concurrency
//
// Design mirrors MC 1.7.10 EntityAITasks:
//   - Each goal has a priority (lower = higher importance)
//   - Goals declare mutex bits to prevent incompatible concurrent execution
//   - A higher-priority goal can preempt lower-priority running goals
//   - Full evaluation is throttled to every 3 ticks to save CPU
//   - Two schedulers per mob: one for behaviors, one for target selection
// =========================================================================

#include <cstdint>
#include <vector>
#include <memory>

namespace ai {

// Mutex bits for task mutual exclusion (MC-style).
// When two goals share any mutex bits, only one can run at a time.
// The higher-priority (lower number) goal wins.
enum MutexBits : uint8_t {
    MUTEX_NONE  = 0,
    MUTEX_MOVE  = 1,   // Movement tasks
    MUTEX_LOOK  = 2,   // Look/head rotation
    MUTEX_SWIM  = 4,   // Swimming
    MUTEX_MOVE_LOOK = MUTEX_MOVE | MUTEX_LOOK // 3 — movement + look
};

// =====================================================================
// Base class for all AI goals
// =====================================================================
class AIGoal {
public:
    virtual ~AIGoal() = default;

    // Can this goal start? (checked every few ticks)
    virtual bool shouldExecute() = 0;

    // Should a running goal keep running? (default: calls shouldExecute)
    virtual bool continueExecuting() { return shouldExecute(); }

    // One-time setup when goal starts
    virtual void startExecuting() {}

    // Called every tick while goal runs
    virtual void updateTask(float dt) {}

    // Cleanup when goal stops
    virtual void resetTask() {}

    // Can higher-priority goals preempt this? (default: true)
    virtual bool isInterruptible() const { return true; }

    // Mutex bits for concurrency control
    void setMutexBits(uint8_t bits) { m_mutexBits = bits; }
    uint8_t getMutexBits() const { return m_mutexBits; }

private:
    uint8_t m_mutexBits = MUTEX_NONE;
};

// =====================================================================
// A prioritized entry in the task list
// =====================================================================
struct AITaskEntry {
    int priority;                   // Lower number = higher priority
    std::unique_ptr<AIGoal> goal;
    bool running = false;
};

// =====================================================================
// Goal-based AI scheduler (MC EntityAITasks equivalent)
// Two instances per mob: one for behaviors, one for targeting
// =====================================================================
class AITaskScheduler {
public:
    // Add a goal with given priority
    void addGoal(int priority, std::unique_ptr<AIGoal> goal);

    // Run the scheduler (called every tick, internally throttles to every 3 ticks)
    void update(float dt);

    // Stop all running goals
    void stopAll();

private:
    std::vector<AITaskEntry> m_entries;
    int m_tickCount = 0;

    // Can the task at the given index run without conflicting with higher-priority running tasks?
    bool canUse(size_t idx) const;
};

} // namespace ai
