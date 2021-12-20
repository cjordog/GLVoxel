#include "Input.h"
#include <GLFW/glfw3.h>

Input::Input(Window* w)
	: m_window(w)
{
	glfwSetInputMode(w->GetGLFWWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Input::ProcessInput()
{
	GLFWwindow* glfwWin = m_window->GetGLFWWindow();
	if (glfwGetKey(glfwWin, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(glfwWin, true);

	m_inputData.Reset();

	if (glfwGetKey(glfwWin, GLFW_KEY_W) == GLFW_PRESS)
		m_inputData.m_moveInput.z -= 1.0f;
	if (glfwGetKey(glfwWin, GLFW_KEY_S) == GLFW_PRESS)
		m_inputData.m_moveInput.z += 1.0f;
	if (glfwGetKey(glfwWin, GLFW_KEY_A) == GLFW_PRESS)
		m_inputData.m_moveInput.x -= 1.0f;
	if (glfwGetKey(glfwWin, GLFW_KEY_D) == GLFW_PRESS)
		m_inputData.m_moveInput.x += 1.0f;
	if (glfwGetKey(glfwWin, GLFW_KEY_SPACE) == GLFW_PRESS)
		m_inputData.m_moveInput.y += 1.0f;
	if (glfwGetKey(glfwWin, GLFW_KEY_Z) == GLFW_PRESS)
		m_inputData.m_moveInput.y -= 1.0f;

	double xpos, ypos;
	glfwGetCursorPos(glfwWin, &xpos, &ypos);
	
	if (!m_isFirstFrame)
	{
		m_inputData.m_mouseInput.x = float(xpos - m_lastCursorPosX);
		m_inputData.m_mouseInput.y = float(ypos - m_lastCursorPosY);
	}

	m_lastCursorPosX = xpos;
	m_lastCursorPosY = ypos;

	m_inputData.m_mouseButtons.x = (glfwGetMouseButton(glfwWin, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS);
	m_inputData.m_mouseButtons.y = (glfwGetMouseButton(glfwWin, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS);

	m_isFirstFrame = false;
}

void Input::Poll()
{
	glfwPollEvents();
}