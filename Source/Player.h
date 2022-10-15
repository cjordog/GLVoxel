#pragma once

#include <glm/vec3.hpp>
#include "Camera.h"
#include "BoxCollider.h"

class Player
{
public:
	Player(const glm::vec3& position);

	const Camera& GetCamera() { return m_camera; }
	BoxCollider& GetBoxCollider() { return m_collider; }
private:
	glm::vec3 m_position = glm::vec3(0, 100, 0);
	Camera m_camera;
	glm::vec3 m_cameraOffset = glm::vec3(0);
	BoxCollider m_collider;

	const glm::vec3 m_sizeVoxels = glm::vec3(2, 4, 2);
};