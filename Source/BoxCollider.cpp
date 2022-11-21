#include "BoxCollider.h"

BoxCollider::BoxCollider()
	: BoxCollider(glm::vec3(0), glm::vec3(1))
{
}

BoxCollider::BoxCollider(const glm::vec3& centerPosition, const glm::vec3& size)
	: m_AABB(centerPosition, size.x, size.y, size.z),
	Collider()
{
	m_velocity = glm::vec3(0);
	m_desiredPosition = GetPosition();
}

const glm::vec3 BoxCollider::GetPosition() const
{
	glm::vec3 halfExtents = (m_AABB.max - m_AABB.min) / 2.0f;
	return (m_AABB.min + glm::vec3(halfExtents.x, 0.0f, halfExtents.z));
}
