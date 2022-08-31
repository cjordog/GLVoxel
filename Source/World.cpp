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

#ifdef DEBUG
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#endif

ShaderProgram World::shaderProgram1;

World::World()
	: m_camera(glm::vec3(0, 80, -10), 0, 90.0f),
	m_frozenCamera(glm::vec3(0, 80, -10), 0, 90.0f),
	m_voxelScene()
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
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef DEBUG
	ImGuiBeginRender();
#endif

	//shaderProgram1.Use();

	//glBindVertexArray(VAO);
	//glBindBuffer(GL_ARRAY_BUFFER, VBO);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	//glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D, testTex1);
	//glActiveTexture(GL_TEXTURE1);
	//glBindTexture(GL_TEXTURE_2D, testTex2);

	//glUniform1i(512, 0);
	//glUniform1i(513, 1);

	//glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.f);
	////glm::mat4 View = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
	////glm::mat4 Model = glm::rotate(glm::mat4(1.0f), 30.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	//glm::mat4 Model = glm::mat4(1.0f);

	//glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_camera.GetViewMatrix() * Model));
	//glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));

	//glBindVertexArray(VAO);
	//glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	m_voxelScene.Render(&m_camera, &m_frozenCamera);

#ifdef DEBUG
	ImGuiRender();
#endif
}

void World::Update(float updateTime, InputData* inputData)
{
	m_speed += inputData->m_mouseWheel.y * 2.0f;
	UpdateCamera(updateTime, inputData);
	m_voxelScene.Update(m_camera.GetPosition(), m_debugParams);

	if (m_debugParams.m_validateThisFrame)
		m_debugParams.m_validateThisFrame = false;

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

#ifdef DEBUG
void World::ImGuiBeginRender()
{
	// feed inputs to dear imgui, start new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void World::ImGuiRender()
{
	// render your GUI
	ImGui::Begin("Demo window");
	ImGui::Text("Framerate: %d", m_frameRate);
	glm::vec3 cameraPos = m_camera.GetPosition();
	ImGui::Text("Position x:%.2f y:%.2f z:%.2f", cameraPos.x, cameraPos.y, cameraPos.z);
	ImGui::Checkbox("Freeze Camera", &m_freezeCamera);
	ImGui::Checkbox("Validate", &m_debugParams.m_validateThisFrame);
	ImGui::Text("Rendering %d vertices", VoxelScene::s_numVerts);
	//ImGui::ShowDemoWindow();
	ImGui::End();

	// Render dear imgui into screen
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
#endif