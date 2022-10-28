#pragma once
#include "Collider.h"
#include "Common.h"
#include <glm/vec3.hpp>

class BoxCollider : public Collider
{
public:
	BoxCollider();
	BoxCollider(const glm::vec3& centerPosition, const glm::vec3& size);

	const glm::vec3 GetPosition() const override { return (m_AABB.center - glm::vec3(0.0f, m_AABB.extents.y, 0.0f)); }
	void Translate(const glm::vec3& pos) override { m_AABB.center += pos; m_desiredPosition = GetPosition(); }
	void TranslateAndResolve(const glm::vec3& pos) override { m_desiredPosition += pos; }
private:
	AABB m_AABB;
};