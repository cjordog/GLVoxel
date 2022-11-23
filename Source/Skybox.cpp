#include "Skybox.h"
#include <glad/glad.h>

ShaderProgram Skybox::s_skyboxShaderProgram;
uint Skybox::s_VAO = 0;
uint Skybox::s_VBO = 0;

float skyboxVertices[] = {
	// positions          
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f
};

Skybox::Skybox(uint cubeMapID)
{
	m_textureID = cubeMapID;
}

Skybox::Skybox()
{

}

void Skybox::InitShared()
{
	s_skyboxShaderProgram = ShaderProgram("skybox.vs.glsl", "skybox.fs.glsl");

	glGenVertexArrays(1, &s_VAO);
	glBindVertexArray(s_VAO);

	glGenBuffers(1, &s_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
	glBufferData(GL_ARRAY_BUFFER, 36 * sizeof(VertexP), (float*)skyboxVertices, GL_STATIC_DRAW);

	constexpr int vertexBindingPoint = 0;
	glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexAttribBinding(0, vertexBindingPoint);
	glEnableVertexAttribArray(0);
}

void Skybox::Render(const Camera& camera)
{
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	s_skyboxShaderProgram.Use();

	glUniformMatrix4fv(0, 1, GL_FALSE, &camera.GetViewMatrix()[0][0]);
	glUniformMatrix4fv(1, 1, GL_FALSE, &camera.GetProjMatrix()[0][0]);

	glBindVertexArray(s_VAO);
	glBindVertexBuffer(0, s_VBO, 0, sizeof(VertexP));
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
}

