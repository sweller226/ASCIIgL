#include <ASCIICraft/mobs/Mobs.hpp>
#include <ASCIICraft/world/World.hpp>

#include <ASCIIgL/util/Logger.hpp>

using namespace ASCIICraft;

// Constructor: initialize the mob's `position` via initializer list
// -> Using initializer list constructs `position` directly from `pos`
Mobs::Mobs(const glm::vec3& pos)
	: position(pos)
{}

// Virtual destructor: defaulted implementation
Mobs::~Mobs() = default;

// Render: default no-op
// -> Derived classes may override to implement rendering
// -> Marked `const` because render shouldn't modify mob state
void Mobs::Render() const
{
	// -> Intentionally empty; renderer should query position/type and sprite info
}

// TakeDamage: subtract health and handle death
// -> Guards against damaging a dead mob
// -> When health <= 0, mark `alive = false` and log death
void Mobs::TakeDamage(float amount)
{
	if (!alive) return;            // -> no-op for dead mobs
	health -= amount;              // -> subtract health
	if (health <= 0.0f) {          // -> handle death
		alive = false;
		ASCIIgL::Logger::Info("Mobs: died (type=" + type + ")");
	}
}

// IsAlive: return whether the mob is alive
bool Mobs::IsAlive() const
{
	return alive;
}

// Position accessors
// -> `GetPosition` returns a const reference to avoid a copy
// -> caller must not use the reference after the mob is destroyed
const glm::vec3& Mobs::GetPosition() const { return position; }
void Mobs::SetPosition(const glm::vec3& pos) { position = pos; }

// Type accessors for debugging
const std::string& Mobs::GetType() const { return type; }
void Mobs::SetType(const std::string& t) { type = t; }