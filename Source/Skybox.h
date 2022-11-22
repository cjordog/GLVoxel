#pragma once

#include "Common.h"
#include "Camera.h"
#include "ShaderProgram.h"

#include <vector>

class Skybox
{
public:
	Skybox();
	Skybox(uint cubeMapID);

	static void InitShared();

	void SetTextureID(uint id) { m_textureID = id; }
	void Render(const Camera& camera);

private:
	uint m_textureID = 0;

	static ShaderProgram s_skyboxShaderProgram;
	static std::vector<VertexP> s_skyboxVertices;
	static uint s_VAO;
	static uint s_VBO; 
	static uint s_EBO;
};