#pragma once
#include "Collider.h"
#include "Common.h"
#include <glm/vec3.hpp>

class BoxCollider : public Collider
{
public:
	BoxCollider();
	BoxCollider(const glm::vec3& centerPosition, const glm::vec3& size);

	glm::vec3 GetMiddleCenter() { return m_AABB.center - glm::vec3(0, m_AABB.extents.y, 0); }
	void SetMiddleCenter(const glm::vec3& pos) { m_AABB.center = pos + glm::vec3(0, m_AABB.extents.y, 0); }
private:
	AABB m_AABB;
};