// =========================================================================
// AIGoal.cpp — AITaskScheduler implementation
//
// Tick flow:
//   1. Every tick: check if running goals should stop (continueExecuting)
//   2. Every 3 ticks (full evaluation): check if idle goals should start
//   3. On start: preempt any conflicting lower-priority running goals
//   4. Every tick: call updateTask() on all running goals
//
// The 3-tick throttle on shouldExecute() prevents expensive per-tick checks
// (e.g. pathfinding, entity scanning) from tanking frame rate.
// =========================================================================

#include <ASCIICraft/ai/AIGoal.hpp>

namespace ai {

void AITaskScheduler::addGoal(int priority, std::unique_ptr<AIGoal> goal) {
    AITaskEntry entry;
    entry.priority = priority;
    entry.goal = std::move(goal);
    entry.running = false;
    m_entries.push_back(std::move(entry));
}

void AITaskScheduler::update(float /* dt */) {
    ++m_tickCount;

    // Full evaluation every 3 ticks (MC-style throttle).
    // Running tasks are checked every tick, but starting new tasks
    // is deferred to reduce shouldExecute() call frequency.
    bool fullUpdate = (m_tickCount % 3 == 0);

    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = m_entries[i];

        if (entry.running) {
            // Check if running task should continue
            if (!entry.goal->continueExecuting() || !canUse(i)) {
                entry.goal->resetTask();
                entry.running = false;
            }
        } else if (fullUpdate) {
            // Check if non-running task should start
            if (canUse(i) && entry.goal->shouldExecute()) {
                // Preempt conflicting lower-priority running tasks
                uint8_t myMutex = entry.goal->getMutexBits();
                for (size_t j = 0; j < m_entries.size(); ++j) {
                    if (j == i) continue;
                    if (m_entries[j].running &&
                        (m_entries[j].goal->getMutexBits() & myMutex) != 0 &&
                        m_entries[j].priority > entry.priority &&
                        m_entries[j].goal->isInterruptible()) {
                        m_entries[j].goal->resetTask();
                        m_entries[j].running = false;
                    }
                }
                entry.goal->startExecuting();
                entry.running = true;
            }
        }
    }

    // Update all running tasks
    for (auto& entry : m_entries) {
        if (entry.running) {
            entry.goal->updateTask(1.0f / 30.0f); // fixed dt matching physics
        }
    }
}

bool AITaskScheduler::canUse(size_t idx) const {
    uint8_t myMutex = m_entries[idx].goal->getMutexBits();
    int myPriority = m_entries[idx].priority;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (i == idx) continue;
        if (!m_entries[i].running) continue;

        // Compatible if no overlap in mutex bits
        if ((m_entries[i].goal->getMutexBits() & myMutex) == 0) continue;

        // Conflicting — can only run if we have higher priority (lower number)
        // and the other task is interruptible
        if (myPriority < m_entries[i].priority && m_entries[i].goal->isInterruptible()) {
            continue; // We can preempt it
        }

        return false; // Can't run
    }
    return true;
}

void AITaskScheduler::stopAll() {
    for (auto& entry : m_entries) {
        if (entry.running) {
            entry.goal->resetTask();
            entry.running = false;
        }
    }
}

} // namespace ai
