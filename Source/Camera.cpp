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
	//if (glm::length(posDelta) == 0 && pitchDelta == 0 && yawDelta == 0)
	//	return;

	m_yaw += yawDelta;
	m_pitch += pitchDelta;

	constexpr float maxAngle = 89.0f;
	m_pitch = glm::clamp(m_pitch, -maxAngle, maxAngle);

	m_forward.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_forward.y = sin(glm::radians(m_pitch));
	m_forward.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

	glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	m_forward = glm::normalize(m_forward);
	m_right = (normalize(cross(m_forward, worldUp)));
	m_up = (cross(m_right, m_forward));

	m_viewMatrix = glm::lookAt(m_position, m_position + m_forward, worldUp);

	m_position += posDelta * glm::mat3(m_viewMatrix);

	m_halfVSide = m_farClip * tanf(GetFovY() * 0.5f);
	m_halfHSide = m_halfVSide * m_aspectRatio;
	
	m_movedThisFrame = true;
}

void Camera::CalculateFrustum()
{
	const glm::vec3 frontMultFar = m_farClip * m_forward;
	m_frustum.nearFace = { m_position + m_nearClip * m_forward, m_forward };
	m_frustum.farFace = { m_position + frontMultFar, -m_forward };
	m_frustum.rightFace = { m_position, glm::cross(m_up, frontMultFar + m_right * m_halfHSide) };
	m_frustum.leftFace = { m_position, glm::cross(frontMultFar - m_right * m_halfHSide, m_up) };
	m_frustum.topFace = { m_position, glm::cross(m_right, frontMultFar - m_up * m_halfVSide) };
	m_frustum.bottomFace = { m_position, glm::cross(frontMultFar + m_up * m_halfVSide, m_right) };
}

void Camera::FrameStart()
{
	m_movedThisFrame = false;
}
