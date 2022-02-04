#pragma once

#include "Common.h"
#include "ShaderProgram.h"
#include "Camera.h"
#include "VoxelScene.h"

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

private:
	uint CalcFrameRate();

	static ShaderProgram shaderProgram1;
	uint VAO = 0;
	uint VBO = 0;
	uint EBO = 0;

	uint testTex1 = 0, testTex2 = 0;

	Camera m_camera;

	VoxelScene m_voxelScene;

	// this should be averaged over time, not frames
	std::deque<float> m_frameTimes;
	uint m_frameRate = 0;
};