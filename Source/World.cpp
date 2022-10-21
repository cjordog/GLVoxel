#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>
#include "World.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef IMGUI_ENABLED
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

ShaderProgram World::shaderProgram1;

World::World()
	: m_camera(glm::vec3(0, 80, -10), 0, 90.0f),
	m_frozenCamera(glm::vec3(0, 80, -10), 0, 90.0f),
	m_voxelScene(),
	m_player(glm::vec3(0, 100, 0))
{
	m_flags |= WorldFlags::EnableMT;
}

bool World::Init()
{
	testTex1 = LoadTexture("uv-test.png", ImageFormat::PNG);
	testTex2 = LoadTexture("woodbox.jpg", ImageFormat::JPG);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	return true;
}

bool World::InitShared()
{
	shaderProgram1 = ShaderProgram("test1.vs.glsl", "test1.fs.glsl");
	VoxelScene::InitShared();
	return true;
}

void World::Render()
{
	ZoneScoped;

	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef IMGUI_ENABLED
	ImGuiBeginRender();
#endif

	m_voxelScene.Render(&m_player.GetCamera(), &m_player.GetCamera());

#ifdef IMGUI_ENABLED
	ImGuiRenderStart();
	m_voxelScene.RenderImGui();
	ImGuiRenderEnd();
#endif
}

void World::Update(float updateTime, InputData* inputData)
{
	ZoneScoped;
	m_speed += inputData->m_mouseWheel.y * 2.0f;
	UpdateCamera(updateTime, inputData);
	UpdatePhysics(updateTime);
	UpdatePlayer(updateTime, inputData);
	m_voxelScene.Update(m_player.GetCamera().GetPosition());

	CalcFrameRate(updateTime);
}

uint World::LoadTexture(const char* image, ImageFormat fmt)
{
	uint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// load and generate the texture
	int width, height, nrChannels;
	char imgPath[128];
	strcpy_s(imgPath, "../Assets/Textures/");
	strcat_s(imgPath, image);

	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(imgPath, &width, &height, &nrChannels, 0);
	if (data)
	{
		switch (fmt)
		{
			case ImageFormat::PNG:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				break;
			case ImageFormat::JPG:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
				break;
			default:
				break;
		}
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);
	return texture;
}

constexpr float FRAMERATE_MS = 1000.0f;
uint World::CalcFrameRate(float frameTime)
{
	m_frameTimes.push_back(frameTime);
	m_frameTimeTotal += frameTime;

	while (m_frameTimeTotal > FRAMERATE_MS)
	{
		m_frameTimeTotal -= m_frameTimes.front();
		m_frameTimes.pop_front();
	}

	m_frameRate = uint(m_frameTimes.size() / FRAMERATE_MS * 1000.f);
	return m_frameRate;
}

void World::UpdateCamera(float updateTime, InputData* inputData)
{
	m_camera.FrameStart();
	if (!inputData->m_disableMouseLook)
	{
		// TODO:: camera should probably be transformed right before render and elapsed time calculated then, so long frames dont cause jumps on the next frame
		m_camera.Transform(inputData->m_moveInput * m_speed * (updateTime / 1000.0f), -inputData->m_mouseInput.y * 0.5f, inputData->m_mouseInput.x * 0.5f);
	}

	m_camera.CalculateFrustum();

	if (!m_freezeCamera)
	{
		m_frozenCamera = m_camera;
	}
}

// TODO:: should be an update function on player
void World::UpdatePlayer(float updateTime, InputData* inputData)
{
	Camera& camera = m_player.GetCamera();
	camera.FrameStart();
	if (!inputData->m_disableMouseLook)
	{
		// TODO:: camera should probably be transformed right before render and elapsed time calculated then, so long frames dont cause jumps on the next frame
		//camera.Transform(inputData->m_moveInput * m_speed * (updateTime / 1000.0f), -inputData->m_mouseInput.y * 0.5f, inputData->m_mouseInput.x * 0.5f);
		camera.Transform(m_player.GetBoxCollider().GetMiddleCenter() + m_player.GetCameraOffset(), -inputData->m_mouseInput.y * 0.5f, inputData->m_mouseInput.x * 0.5f);
	}
	camera.CalculateFrustum();
}

void World::UpdatePhysics(float timeDelta)
{
	m_voxelScene.ResolveBoxCollider(m_player.GetBoxCollider(), timeDelta);
}

#ifdef IMGUI_ENABLED
void World::ImGuiBeginRender()
{
	// feed inputs to dear imgui, start new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void World::ImGuiRenderStart()
{
	// render your GUI
	ImGui::Begin("Demo window");
	ImGui::Text("Framerate: %d", m_frameRate);
	glm::vec3 cameraPos = m_player.GetCamera().GetPosition();
	ImGui::Text("Position x:%.2f y:%.4f z:%.2f", cameraPos.x, cameraPos.y, cameraPos.z);
	ImGui::Checkbox("Freeze Camera", &m_freezeCamera);
}

void World::ImGuiRenderEnd()
{
	//ImGui::ShowDemoWindow();
	ImGui::End();

	// Render dear imgui into screen
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
#endif