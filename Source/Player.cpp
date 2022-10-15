#include "Player.h"

#include "Common.h"

Player::Player(const glm::vec3& position)
	: m_camera(position, 0, 90.0f)
{
	// position is center bottom. different from everything else... idk
	m_position = position;
	m_cameraOffset = glm::vec3(0, (m_sizeVoxels.y - 0.5f) * VOXEL_UNIT_SIZE, 0);
	m_collider = BoxCollider(m_position + m_sizeVoxels.y * 0.5f * VOXEL_UNIT_SIZE, m_sizeVoxels * VOXEL_UNIT_SIZE);
}
