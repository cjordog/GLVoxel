#pragma once

#include "Common.h"
#include "Camera.h"
#include "Player.h"
#include "ShaderProgram.h"
#include "VoxelScene.h"
#include "Skybox.h"

#include <deque>

class World
{
public:
	World();

	static bool InitShared();
	bool Init();
	void Render();
	void Update(float updateTime, InputData* inputData);
	enum class ImageFormat {
		PNG = 0,
		JPG = 1,
	};
	uint LoadTexture(const char* image, ImageFormat fmt);
	uint LoadCubemap(const std::vector<std::string>& faces);

private:
	uint CalcFrameRate(float frameTime);
	void UpdateCamera(float updateTime, InputData* inputData);
	void UpdatePlayer(float updateTime, InputData* inputData);
	void UpdatePhysics(float timeDelta);
#ifdef IMGUI_ENABLED
	void ImGuiBeginRender();
	void ImGuiRenderStart();
	void ImGuiRenderEnd();
#endif

	static ShaderProgram shaderProgram1;
	uint VAO = 0;
	uint VBO = 0;
	uint EBO = 0;

	uint testTex1 = 0, testTex2 = 0;

	Camera m_frozenCamera;

	VoxelScene m_voxelScene;

	std::deque<float> m_frameTimes;
	float m_frameTimeTotal = 0;
	uint m_frameRate = 0;

	bool m_freezeCamera = false;

	uint m_flags = 0;

	//float m_speed = 10.0f;

	Player m_player;
	Skybox m_skybox;
};