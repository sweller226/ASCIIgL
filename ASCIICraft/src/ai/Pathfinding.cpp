// =========================================================================
// Pathfinding.cpp — A* implementation
//
// Algorithm notes:
//   - Manhattan heuristic (optimal for 4-directional grids)
//   - Step-up (+1 block) costs 1.5, fall costs 1.0 + 0.1 per block dropped
//   - 1000-iteration cap prevents runaway searches in complex terrain
//   - Falls back to closest-to-goal node when the goal is unreachable
//
// The pathfinder only generates cardinal (N/S/E/W) waypoints.
// Diagonal movement is handled at the AIBehaviors level via followPath(),
// which beelines to the final destination on flat ground.
// =========================================================================

#include <ASCIICraft/ai/Pathfinding.hpp>
#include <ASCIICraft/world/ChunkManager.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>

namespace ai {

// =====================================================================
// Hash a block coordinate for the open/closed sets
// Uses boost-style hash combine (0x9e3779b9 = golden ratio constant)
// =====================================================================
struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        size_t h = 0;
        h ^= std::hash<int>()(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// =====================================================================
// A* node
// =====================================================================
struct Node {
    glm::ivec3 pos;
    float g;   // cost from start
    float f;   // g + heuristic
    glm::ivec3 parent;
    bool hasParent;
};

struct NodeCmp {
    bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
};

// =====================================================================
// Block solidity check
// =====================================================================
bool Pathfinder::isSolid(
    const ChunkManager* cm,
    const blockstate::BlockStateRegistry& bsr,
    int x, int y, int z)
{
    uint32_t sid = cm->GetBlockState(x, y, z);
    return bsr.GetState(sid).isSolid;
}

// =====================================================================
// Can an entity stand at (x, y, z)?
// Needs solid ground at y-1 and clear space for the entity body at y..y+height-1
// =====================================================================
bool Pathfinder::canStandAt(
    const ChunkManager* cm,
    const blockstate::BlockStateRegistry& bsr,
    int x, int y, int z,
    int widthBlocks, int heightBlocks)
{
    // Check ground below
    int halfW = widthBlocks / 2;
    bool hasGround = false;
    for (int dx = -halfW; dx <= halfW; ++dx) {
        for (int dz = -halfW; dz <= halfW; ++dz) {
            if (isSolid(cm, bsr, x + dx, y - 1, z + dz)) {
                hasGround = true;
            }
        }
    }
    if (!hasGround) return false;

    // Check body space is clear
    for (int dy = 0; dy < heightBlocks; ++dy) {
        for (int dx = -halfW; dx <= halfW; ++dx) {
            for (int dz = -halfW; dz <= halfW; ++dz) {
                if (isSolid(cm, bsr, x + dx, y + dy, z + dz)) {
                    return false;
                }
            }
        }
    }
    return true;
}

// =====================================================================
// A* pathfinding (MC-style: 4 cardinal directions, step-up/fall-down)
// =====================================================================
Path Pathfinder::findPath(
    const ChunkManager* cm,
    const blockstate::BlockStateRegistry& bsr,
    const glm::ivec3& start,
    const glm::ivec3& goal,
    float entityWidth,
    float entityHeight,
    float maxDistance)
{
    Path result;
    if (!cm) return result;

    int widthBlocks = std::max(1, static_cast<int>(std::ceil(entityWidth)));
    int heightBlocks = std::max(1, static_cast<int>(std::ceil(entityHeight)));

    // Cap search area
    float maxDistSq = maxDistance * maxDistance;

    // A* open set (priority queue) and closed set
    std::priority_queue<Node, std::vector<Node>, NodeCmp> open;
    std::unordered_map<glm::ivec3, Node, IVec3Hash> allNodes;
    std::unordered_set<glm::ivec3, IVec3Hash> closed;

    // Heuristic: Manhattan distance (good for 4-directional)
    auto heuristic = [](const glm::ivec3& a, const glm::ivec3& b) -> float {
        return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z));
    };

    // Start node
    Node startNode;
    startNode.pos = start;
    startNode.g = 0.0f;
    startNode.f = heuristic(start, goal);
    startNode.hasParent = false;
    open.push(startNode);
    allNodes[start] = startNode;

    // Track closest node to goal as fallback
    Node closestNode = startNode;
    float closestDist = heuristic(start, goal);

    // 4 cardinal directions
    static const glm::ivec3 dirs[4] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    };

    int iterations = 0;
    const int maxIterations = 1000; // prevent runaway searches

    while (!open.empty() && iterations < maxIterations) {
        ++iterations;

        Node current = open.top();
        open.pop();

        // Already processed?
        if (closed.count(current.pos)) continue;
        closed.insert(current.pos);

        // Reached goal?
        if (current.pos == goal) {
            closestNode = current;
            break;
        }

        // Track closest to goal
        float distToGoal = heuristic(current.pos, goal);
        if (distToGoal < closestDist) {
            closestDist = distToGoal;
            closestNode = current;
        }

        // Explore neighbors
        for (const auto& dir : dirs) {
            // Try same-level, step-up (+1), and fall-down (up to 4 blocks)
            for (int dy = 1; dy >= -4; --dy) {
                glm::ivec3 neighbor = current.pos + dir + glm::ivec3(0, dy, 0);

                // Skip if too far from start
                glm::vec3 diff = glm::vec3(neighbor - start);
                if (glm::dot(diff, diff) > maxDistSq) continue;

                // Skip if already closed
                if (closed.count(neighbor)) continue;

                // Can the entity stand here?
                if (!canStandAt(cm, bsr, neighbor.x, neighbor.y, neighbor.z, widthBlocks, heightBlocks)) {
                    // For step-up, if we can't stand at +1, don't bother checking 0, -1, etc.
                    if (dy == 1) continue;
                    // For same level and fall, keep trying lower
                    continue;
                }

                // Step-up requires clear space above current position
                if (dy == 1) {
                    bool clearAbove = true;
                    for (int ch = 0; ch < heightBlocks; ++ch) {
                        if (isSolid(cm, bsr, current.pos.x, current.pos.y + heightBlocks + ch, current.pos.z)) {
                            clearAbove = false;
                            break;
                        }
                    }
                    if (!clearAbove) continue;
                }

                // Cost: base 1.0 + extra for vertical movement
                float moveCost = 1.0f;
                if (dy > 0) moveCost = 1.5f; // step up costs more
                if (dy < 0) moveCost = 1.0f + 0.1f * std::abs(dy); // falling

                float newG = current.g + moveCost;
                float newF = newG + heuristic(neighbor, goal);

                // Check if we have a better path already
                auto it = allNodes.find(neighbor);
                if (it != allNodes.end() && it->second.g <= newG) continue;

                Node neighborNode;
                neighborNode.pos = neighbor;
                neighborNode.g = newG;
                neighborNode.f = newF;
                neighborNode.parent = current.pos;
                neighborNode.hasParent = true;

                allNodes[neighbor] = neighborNode;
                open.push(neighborNode);

                // Only use first valid dy for this direction (highest ground)
                break;
            }
        }
    }

    // Reconstruct path from closest node
    if (closestNode.pos == start && closestNode.pos != goal) {
        return result; // No progress possible
    }

    std::vector<PathPoint> reversed;
    glm::ivec3 cur = closestNode.pos;
    while (true) {
        reversed.push_back({cur.x, cur.y, cur.z});
        auto it = allNodes.find(cur);
        if (it == allNodes.end() || !it->second.hasParent) break;
        cur = it->second.parent;
    }

    std::reverse(reversed.begin(), reversed.end());
    result.points = std::move(reversed);
    result.currentIndex = 0;
    return result;
}

} // namespace ai
