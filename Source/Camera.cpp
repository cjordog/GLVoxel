#include "Camera.h"

Camera::Camera(glm::vec3 pos, float pitch, float yaw)
{
	m_position = pos;
	m_pitch = pitch;
	m_yaw = yaw;

	glm::vec3 direction;
	direction.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	direction.y = sin(glm::radians(m_pitch));
	direction.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	direction = glm::normalize(direction);

	m_viewMatrix = glm::lookAt(m_position, m_position + direction, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::Transform(glm::vec3 posDelta, float pitchDelta, float yawDelta)
{
	m_yaw += yawDelta;
	m_pitch += pitchDelta;

	constexpr float maxAngle = 89.0f;
	m_pitch = glm::clamp(m_pitch, -maxAngle, maxAngle);

	glm::vec3 direction;
	direction.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	direction.y = sin(glm::radians(m_pitch));
	direction.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	direction = glm::normalize(direction);

	m_viewMatrix = glm::lookAt(m_position, m_position + direction, glm::vec3(0.0f, 1.0f, 0.0f));

	m_position += posDelta * glm::mat3(m_viewMatrix);
}
