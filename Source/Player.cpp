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

void Player::UpdatePosition(float deltaTime, InputData* inputData)
{
	//if (inputData->m_moveInput != glm::vec3(0))
	{
		glm::vec3 forward = m_camera.GetForward();
		if (m_freeCam)
		{
			m_collider.Translate((forward * -inputData->m_moveInput.z + m_camera.GetRight() * inputData->m_moveInput.x + glm::vec3(0, inputData->m_moveInput.y, 0)) * deltaTime / 1000.0f * m_speed);
		}
		else
		{
			forward.y = 0;
			forward = glm::normalize(forward);
			glm::vec3 vel = (forward * -inputData->m_moveInput.z + m_camera.GetRight() * inputData->m_moveInput.x) * /*deltaTime / 1000.0f **/ m_speed;
			m_collider.SetVelocityX(vel.x);
			m_collider.SetVelocityZ(vel.z);
			if (inputData->m_moveInput.y)
			{
				m_collider.SetVelocityY(50);
			}
		}
	}
}

void Player::UpdateCamera(float deltaTime, InputData* inputData)
{
	m_camera.FrameStart();
	if (!inputData->m_disableMouseLook/* && inputData->m_mouseInput != glm::vec2(0)*/)
	{
		// TODO:: camera should probably be transformed right before render and elapsed time calculated then, so long frames dont cause jumps on the next frame
		//camera.Transform(inputData->m_moveInput * m_speed * (updateTime / 1000.0f), -inputData->m_mouseInput.y * 0.5f, inputData->m_mouseInput.x * 0.5f);
		m_camera.Transform(m_collider.GetPosition() + m_cameraOffset, -inputData->m_mouseInput.y * 0.5f, inputData->m_mouseInput.x * 0.5f);
	}
	m_camera.CalculateFrustum();
}
