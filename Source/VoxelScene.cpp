#include "VoxelScene.h"
#include "Chunk.h"
#include <glad/glad.h>
#include <unordered_map>
#include <iostream>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Camera.h"
#include <math.h>

ShaderProgram VoxelScene::s_shaderProgram;

const uint RENDER_DISTANCE = 5;

VoxelScene::VoxelScene()
{
	m_chunks = std::unordered_map<glm::i32vec3, Chunk*>();
}

void VoxelScene::InitShared()
{
	s_shaderProgram = ShaderProgram("terrain.vs.glsl", "terrain.fs.glsl");
}

void VoxelScene::CreateChunk(const glm::i32vec3& chunkPos)
{
	if (m_chunks[chunkPos])
		return;

	Chunk* chunk = new Chunk(chunkPos);
	m_chunks[chunkPos] = chunk;
}

void VoxelScene::Update(const glm::vec3& position)
{
	// update in concentric circles outwards from position
	for (int i = 0; i <= RENDER_DISTANCE; i++)
	{
		//uint updateDiameter = i * 2 + 1;
		for (int j = -i; j <= i; j++)
		{
			for (int k = -i; k <= i; k++)
			{
				for (int l = -i; l <= i; l++)
				{
					glm::i32vec3 localChunkPos(j, k, l);
					if (sqrt(pow(j,2) + pow(k,2) + pow(l,2)) <= float(i))
					{
						glm::i32vec3 chunkPos = localChunkPos + glm::i32vec3(position / float(CHUNK_UNIT_SIZE));
						CreateChunk(chunkPos);
					}
				}
			}
		}
	}
}

void VoxelScene::Render(Camera* camera)
{
	s_shaderProgram.Use();
	glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.f);
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));

	RenderSettings::DrawMode drawMode = RenderSettings::Get().m_drawMode;

	for (auto& chunk : m_chunks) {
		glm::mat4 Model = glm::translate(glm::mat4(1.0f), glm::vec3(chunk.first) * float(CHUNK_UNIT_SIZE));
		glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * Model));

		chunk.second->Render(drawMode);
	}
}

glm::i32vec3 VoxelScene::ConvertWorldPosToChunkPos(const glm::vec3& worldPos)
{
	return worldPos / float(CHUNK_UNIT_SIZE);
}
