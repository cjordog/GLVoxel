#include "Window.h"
#include "Input.h"
#include "World.h"
#include "GX.h"
#include <GLFW/glfw3.h>

#include <chrono>
#include <iostream>

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

int main()
{
	glfwInit();
	Window window(SCR_WIDTH, SCR_HEIGHT);
	if (window.Init() < 0)
	{
		return -1;
	}
	if (GX::GLADInit() < 0)
	{
		return -1;
	}
	Input input(&window);
	World world;
	World::InitShared();
	world.Init();

	auto lastFrameTime = std::chrono::high_resolution_clock::now();

	while (!window.ShouldClose())
	{
		input.ProcessInput();

		auto currentTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> frameTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;

		world.Update(frameTime.count(), input.GetInputData());
		world.Render();
		GX::SwapBuffers(window.GetGLFWWindow());
		input.Poll();
	}

	glfwTerminate();
	return 0;
}