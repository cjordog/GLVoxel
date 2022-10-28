#pragma once

#include <functional>
#include <glm/vec3.hpp>
#include "Camera.h"
#include "BoxCollider.h"

class Player
{
public:
	Player(const glm::vec3& position);

	void UpdatePosition(float deltaTime, InputData* inputData);
	void UpdateCamera(float deltaTime, InputData* inputData);

	Camera& GetCamera() { return m_camera; }
	BoxCollider& GetBoxCollider() { return m_collider; }
	const glm::vec3& GetCameraOffset() const { return m_cameraOffset; }
	const glm::vec3& GetPosition() const { return m_position; }

	bool GetFreeCam() const { return m_freeCam; }
	void SetFreeCam(bool v) { m_freeCam = v; }
	float GetSpeed() const { return m_speed; }
	void SetSpeed(float val) { m_speed = val; }

	std::function<void(Collider&, float)> m_collisionCallback;

private:
	glm::vec3 m_position = glm::vec3(0, 100, 0);
	Camera m_camera;
	glm::vec3 m_cameraOffset = glm::vec3(0);
	BoxCollider m_collider;
	bool m_freeCam = true;
	float m_speed = 50.0f;

	const glm::vec3 m_sizeVoxels = glm::vec3(2, 4, 2);
};