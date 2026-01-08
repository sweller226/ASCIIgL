#include <ASCIICraft/mobs/Pig.hpp>
#include <ASCIICraft/world/World.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <random>
#include <cmath>

using namespace ASCIICraft;

// Small RNG helper: returns a reference to a single static mt19937 instance
// -> Function ensures RNG initialised on first use and reused afterwards
static std::mt19937& GetRng()
{
    static std::random_device rd;
    static std::mt19937 rng(rd());
    return rng;
}

// Constructor: forward `pos` to `Mobs` and set `type` to "Pig"
Pig::Pig(const glm::vec3& pos)
    : Mobs(pos)
{
    type = "Pig"; 
}

// Default destructor: no raw resources to clean up here
Pig::~Pig() = default;

// Update: simple wandering behaviour
// -> `::World` available for future interactions
void Pig::Update(float dt, ::World& world)
{
    if (!alive) return; // -> skip updates for dead mobs

    // -> countdown until next direction change
    wanderTimer -= dt;
    if (wanderTimer <= 0.0f) {
        // -> pick timer between 0.5 and 2.0 seconds
        std::uniform_real_distribution<float> tdist(0.5f, 2.0f);
        // -> pick an angle uniform on 0..2π
        std::uniform_real_distribution<float> adist(0.0f, 2.0f * 3.14159265358979323846f);
        wanderTimer = tdist(GetRng());
        float angle = adist(GetRng());
        // -> convert angle to XZ direction and scale by speed
        velocity = glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * wanderSpeed;
    }

    // -> integrate position using simple Euler step: pos += velocity * delta time
    position += velocity * dt;
}

// Render: intentionally empty; renderer queries sprite path + position
void Pig::Render() const
{
}

// Return the sprite path (read-only reference)
const std::string& Pig::GetSpritePath() const { return spritePath; }
