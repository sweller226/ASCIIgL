#pragma once

#include <ASCIICraft/mobs/Mobs.hpp>
#include <string>

// Puts the Pig Mob class in the ASCIIICraft namespace
namespace ASCIICraft {

// Declares a class named Pig that publicly inherits from the Mobs class
// -> Public inheritance means a Pig "is a" Mobs and preserves the Mobs public interface
class Pig : public Mobs {
public:

    // Constructor with optional position parameter
    // -> explicit to prevent implicit conversions
    // -> const glm::vec3& pos: is a reference to avoid copying [glm::vec3], const to prevent modification
    // -> default value glm::vec3(0.0f) initializes position to origin (if no argument is provided)
    explicit Pig(const glm::vec3& pos = glm::vec3(0.0f));

    // Declares a virtual destructor; override signals 
    // What is a destructor? 
    // -> Syntax: ~ClassName(). It's a special member function with the same name as the class, prefixed with a tilde ~
    // -> Purpose: Cleans up the object's resources and perform any final actions before the object is destroyed
    ~Pig() override;


    // Per-frame logic function; overrides base class pure virtual function in ASCIICraft/src/mobs/Mobs.cpp
    // -> float dt: is the elasped time since last update (delta time)
    // -> ::World& world is passed so the mob can interact with the game world (collisions, entities, terrain)
    void Update(float dt, ::World& world) override;

    // Render hook
    // -> trailing const means this method can't modify the Pig object
    // -> override again enforces match with base class virtual method
    void Render() const override;

    // Returns the sprite path for rendering
    // -> const std::string&: the function return a reference to a std::string and that refernece is a cosnt
    // -> GetSpritePath(): function name
    // -> trailing const again means this method can't modifty the Pig object
    const std::string& GetSpritePath() const;

private:
    std::string spritePath = "res/mobs/pig.png"; // relative to game resources

    // Simple wandering behaviour parameters
    float wanderSpeed = 1.0f; 
    float wanderTimer = 0.0f;
    glm::vec3 velocity = glm::vec3(0.0f);
    
};

} // namespace ASCIICraft
