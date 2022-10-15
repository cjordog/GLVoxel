#pragma once
#include "Collider.h"
#include "Common.h"
#include <glm/vec3.hpp>

class BoxCollider : Collider
{
public:
	BoxCollider();
	BoxCollider(const glm::vec3& centerPosition, const glm::vec3& size);
private:
	AABB m_AABB;
};