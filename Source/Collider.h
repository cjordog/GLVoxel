#pragma once

#include <glm/vec3.hpp>

class Collider
{
public:
	Collider();

	const glm::vec3& GetVelocity() { return m_velocity; }
	void SetVelocity(const glm::vec3& v) { m_velocity = v; }
	virtual const glm::vec3 GetPosition() const = 0;
	virtual void Translate(const glm::vec3& pos) = 0;
	virtual void TranslateAndResolve(const glm::vec3& pos) = 0;

	glm::vec3 m_velocity = glm::vec3(0);
	glm::vec3 m_desiredPosition = glm::vec3(0);
	glm::vec3 m_collisionMask = glm::vec3(1);
	bool isGrounded = false;
};