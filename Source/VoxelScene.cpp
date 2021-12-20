#include "VoxelScene.h"
#include "Chunk.h"
#include <glad/glad.h>
#include <unordered_map>
#include <iostream>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Camera.h"

ShaderProgram VoxelScene::s_shaderProgram;

VoxelScene::VoxelScene()
{

	m_chunks = std::unordered_map<glm::i32vec3, Chunk*>();
	CreateChunk(0, 0, 0);
}

void VoxelScene::InitShared()
{
	s_shaderProgram = ShaderProgram("terrain.vs.glsl", "terrain.fs.glsl");
}

void VoxelScene::CreateChunk(int x, int y, int z)
{
	Chunk* chunk = new Chunk();
	m_chunks[glm::i32vec3(x,y,z)] = chunk;
}

void VoxelScene::Render(Camera* camera)
{
	s_shaderProgram.Use();
	glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.f);
	glm::mat4 Model = glm::mat4(1.0f);

	glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * Model));
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));
	m_chunks[glm::i32vec3(0, 0, 0)]->Render();
}
