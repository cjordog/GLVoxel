#pragma once

#include <glm/vec3.hpp>

class Collider
{
public:
	Collider();

	const glm::vec3& GetVelocity() { return m_velocity; }
	void SetVelocity(const glm::vec3& v) { m_velocity = v; }

	glm::vec3 m_velocity;
	bool isGrounded = false;
};