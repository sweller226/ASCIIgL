#pragma once
// =========================================================================
// Pathfinding.hpp — A* grid-based pathfinder for mob navigation
//
// Operates on the voxel block grid via ChunkManager lookups.  Supports:
//   - 4 cardinal directions (no diagonal neighbours)
//   - Step-up by 1 block, fall-down up to 4 blocks
//   - Entity bounding-box clearance checks
//   - Closest-node fallback when goal is unreachable
//
// Usage:
//   Path p = Pathfinder::findPath(cm, bsr, start, goal);
//   while (!p.isFinished()) { auto wp = p.getCurrentPos(); p.advance(); }
// =========================================================================

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

class ChunkManager;
namespace blockstate { class BlockStateRegistry; }

namespace ai {

// A single waypoint in a computed path (block coordinates)
struct PathPoint {
    int x, y, z;  // block-space position
};

// An ordered list of waypoints from A to B, with a cursor tracking progress.
// The path may end at the closest reachable node if the goal is unreachable.
struct Path {
    std::vector<PathPoint> points;
    int currentIndex = 0;

    bool isFinished() const { return currentIndex >= static_cast<int>(points.size()); }

    // Get the current target world position (block center: +0.5 on X/Z)
    glm::vec3 getCurrentPos() const {
        if (isFinished()) return glm::vec3(0.0f);
        auto& p = points[currentIndex];
        return glm::vec3(p.x + 0.5f, static_cast<float>(p.y), p.z + 0.5f);
    }

    void advance() { ++currentIndex; }
    int remaining() const { return static_cast<int>(points.size()) - currentIndex; }
};

// A* pathfinder on the block grid
// 4-directional, supports step-up by 1, fall-down, entity size checking
class Pathfinder {
public:
    // Find a path from start to goal
    // entityWidth/entityHeight define the bounding volume in blocks
    // maxDistance limits the search radius
    // Returns empty path if no route found
    static Path findPath(
        const ChunkManager* cm,
        const blockstate::BlockStateRegistry& bsr,
        const glm::ivec3& start,
        const glm::ivec3& goal,
        float entityWidth = 0.6f,
        float entityHeight = 1.8f,
        float maxDistance = 32.0f
    );

    // Check if a block is solid (walkable surface).
    // Public so AI behaviors (e.g. KeepDistanceGoal) can do edge-avoidance checks.
    static bool isSolid(
        const ChunkManager* cm,
        const blockstate::BlockStateRegistry& bsr,
        int x, int y, int z
    );

private:
    // Check if an entity can stand at the given block position
    static bool canStandAt(
        const ChunkManager* cm,
        const blockstate::BlockStateRegistry& bsr,
        int x, int y, int z,
        int widthBlocks, int heightBlocks
    );
};

} // namespace ai
