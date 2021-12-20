#pragma once

struct GLFWwindow;

class GX
{
public:
	static int GLADInit();
	static void SwapBuffers(GLFWwindow* window);
};