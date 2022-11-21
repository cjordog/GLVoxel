#pragma once
#include "Collider.h"
#include "Common.h"
#include <glm/vec3.hpp>

class BoxCollider : public Collider
{
public:
	BoxCollider();
	BoxCollider(const glm::vec3& centerPosition, const glm::vec3& size);

	const glm::vec3 GetPosition() const override;
	void Translate(const glm::vec3& pos) override { m_AABB.Translate(pos); m_desiredPosition = GetPosition(); }
	void TranslateAndResolve(const glm::vec3& pos) override { m_desiredPosition += pos; }
	const AABB& GetAABB() { return m_AABB; }
	void SetSlideMask(int index) { slideMask |= 1 << index; };
	bool GetSlideMask(int index) { return (slideMask & 1 << index) != 0; };
	void ClearSlideMask() { slideMask = 0; }
	void ClearSlideMask(int index) { slideMask &= ~(1 << index); }
private:
	AABB m_AABB;
	uint8_t slideMask = 0;
};