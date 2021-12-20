#pragma once

#include "Common.h"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
	Camera(glm::vec3 pos, float pitch, float yaw);

	void Transform(glm::vec3 posDelta, float pitchDelta, float yawDelta);
	glm::mat4 GetViewMatrix() { return m_viewMatrix; }

private:
	glm::vec3 m_position;
	glm::mat4 m_viewMatrix;
	float m_pitch = 0.0f;
	float m_yaw = 90.0f;

	float m_aspectRatio = 4.0f / 3.0f;
	float m_fov = 80.0f;
};