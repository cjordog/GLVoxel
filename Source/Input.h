#pragma once
#include "Window.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "Common.h"

class Input
{
public:
	Input(Window* w);
	InputData* GetInputData(){ return &m_inputData; };
	void ProcessInput();
	void ProcessMousePos(GLFWwindow* window, double xpos, double ypos);
	void Poll();
	//void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

private:
	Window* m_window;
	InputData m_inputData;
	double m_lastCursorPosX = 0;
	double m_lastCursorPosY = 0;

	bool m_isFirstFrame = true;
};