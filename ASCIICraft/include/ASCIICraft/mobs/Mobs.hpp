#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

// Forward declaration of the global World type
// -> Mobs methods accept the global `World&` (defined in World.hpp)
class World;

// Puts the Mobs class in the ASCIICraft namespace
namespace ASCIICraft {

// Declares an abstract base class named Mobs
class Mobs {
public:
	// Construct a generic mob at `pos`
    // -> explicit prevents uninteded implicit conversions
    // -> const glm::vec3& pos: is a reference to avoid copying [glm::vec3], const to prevent modification
    // -> default value glm::vec3(0.0f) initializes position to origin
	explicit Mobs(const glm::vec3& pos = glm::vec3(0.0f));

    // Virtual destructor to allow proper cleanup of derived classes
	virtual ~Mobs();

	// Core lifecycle: update called by the world/game loop. Must be implemented by concrete mobs.
	// -> `::World&` explicitly refers to the global World type (not ASCIICraft::World)
	virtual void Update(float dt, ::World& world) = 0;

	// Optional render hook; default is no-op so render code can be implemented by game/renderer.
	virtual void Render() const;

	// Health/damage handling with a simple default implementation.
	virtual void TakeDamage(float amount);
	bool IsAlive() const;

	// Position accessors
	const glm::vec3& GetPosition() const;
	void SetPosition(const glm::vec3& pos);

	// Provides a human-readable type identifier for the mob (e.g., "Pig", "Zombie")
	const std::string& GetType() const;
	void SetType(const std::string& t);

protected:
	glm::vec3 position;
	float health = 100.0f;
	bool alive = true;
	std::string type = "Mobs";
};

} // namespace ASCIICraft