#include "VoxelScene.h"
#include "Chunk.h"
#include <glad/glad.h>
#include <unordered_map>
#include <unordered_set>
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

Chunk* VoxelScene::CreateChunk(const glm::i32vec3& chunkPos)
{
	if (m_chunks[chunkPos])
		return nullptr;

	Chunk* chunk = new Chunk(chunkPos);
	m_chunks[chunkPos] = chunk;
	
	return chunk;
}

void VoxelScene::Update(const glm::vec3& position)
{
	std::unordered_set<Chunk*> generateMeshList;
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
						Chunk* newChunk = CreateChunk(chunkPos);

						if (newChunk)
						{
							newChunk->GenerateMesh();
							generateMeshList.emplace(newChunk);

							// notify neighbors
							if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Front])])
								if (neighbor->UpdateNeighborRef(BlockFace::Back, newChunk))
									generateMeshList.emplace(neighbor);
							if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Back])])
								if (neighbor->UpdateNeighborRef(BlockFace::Front, newChunk))
									generateMeshList.emplace(neighbor);
							if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Right])])
								if (neighbor->UpdateNeighborRef(BlockFace::Left, newChunk))
									generateMeshList.emplace(neighbor);
							if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Left])])
								if (neighbor->UpdateNeighborRef(BlockFace::Right, newChunk))
									generateMeshList.emplace(neighbor);
							if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Top])])
								if (neighbor->UpdateNeighborRef(BlockFace::Bottom, newChunk))
									generateMeshList.emplace(neighbor);
							if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Bottom])])
								if (neighbor->UpdateNeighborRef(BlockFace::Top, newChunk))
									generateMeshList.emplace(neighbor);
						}
					}
				}
			}
		}
	}
	for (auto& chunk : generateMeshList)
	{
		chunk->GenerateMesh();
	}
}

void VoxelScene::Render(Camera* camera)
{
	s_shaderProgram.Use();
	glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.f);
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));

	RenderSettings::DrawMode drawMode = RenderSettings::Get().m_drawMode;

	for (auto& item : m_chunks) 
	{
		const Chunk* chunk = item.second;
		const glm::vec3& chunkPos = item.first;

		if (chunk == nullptr)
			continue;

		if (chunk->IsEmpty())
			continue;

		glm::mat4 Model = glm::translate(glm::mat4(1.0f), glm::vec3(chunkPos) * float(CHUNK_UNIT_SIZE));
		glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * Model));

		chunk->Render(drawMode);
	}
}

glm::i32vec3 VoxelScene::ConvertWorldPosToChunkPos(const glm::vec3& worldPos)
{
	return worldPos / float(CHUNK_UNIT_SIZE);
}
