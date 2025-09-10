#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/engine/GameObj.hpp>

class Player
{
private:
    static inline float nearClip = 1.0f;
    static inline float farClip = 2000.0f;  
    static inline float fov = 80.0f;

    static inline float walkingSpeed = 70.0f;
    static inline float sprintFactor = 1.5f;
    static inline float cameraTurnRate = 200.0f;

    static inline float maxStamina = 500;
    static inline float stamina = maxStamina;
    static inline float staminaRegen = 2;
    static inline float staminaLoss = 3;
    static inline bool tired = false;
    
    static inline float playerHeight = 20;
    static inline float playerHitBoxRad = 15;

	Camera3D camera;
public:
    Player(glm::vec2 xz, glm::vec2 yawPitch);
    ~Player();

    glm::vec3 GetPlayerPos();
    glm::vec3 GetMoveVector();
    glm::vec2 GetViewChange();
	Camera3D& GetCamera();
	static float GetPlayerHitBoxRad();
	unsigned int GetStaminaChunk(unsigned int numChunks, int leeway);

    glm::vec3 Sprinting(glm::vec3 move);
    bool CollideLevel(glm::vec3 move, GameObj* Level); 
    void Update(GameObj* Level);
	void ResetStamina();
};