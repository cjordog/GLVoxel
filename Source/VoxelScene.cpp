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

const uint RENDER_DISTANCE = 10;

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

	// TODO:: use smart pointers
	Chunk* chunk = new Chunk(chunkPos);
	m_chunks[chunkPos] = chunk;
	chunk->m_generateMeshCallback = std::bind(&VoxelScene::AddToMeshListCallback, this, std::placeholders::_1);
	
	return chunk;
}

inline void VoxelScene::NotifyNeighbor(Chunk* chunk, glm::i32vec3 pos, BlockFace side, BlockFace oppositeSide)
{
	if (Chunk* neighbor = m_chunks[pos + glm::i32vec3(s_blockNormals[side])])
	{
		if (neighbor->UpdateNeighborRef(oppositeSide, chunk))
			m_generateMeshList.push(neighbor);
		if (chunk->UpdateNeighborRef(side, neighbor))
			m_generateMeshList.push(chunk);
	}
}

void VoxelScene::Update(const glm::vec3& position)
{
	GenerateChunks(position);
	GenerateMeshes();

	if (!RenderSettings::Get().mtEnabled)
	{
		Job j;
		while (m_threadPool.GetJob(j))
		{
			j.func();
		}
	}
}

void VoxelScene::GenerateChunks(const glm::vec3& position)
{
	// TODO:: can dynamically update this based on load
	if (currentGenerateRadius != RENDER_DISTANCE)
		currentGenerateRadius++;
	//TODO:: update with real values
	//if (glm::length(lastGeneratePos - position) < 0.0f && lastGenerateRadius == currentGenerateRadius)
	//{
	//	// do something (nothing)
	//}
	//else
	//{
	//	lastGeneratePos = position;
	//	lastGenerateRadius = currentGenerateRadius;
	//}

	// TODO:: fix this?
	//if (RenderSettings::Get().deleteMesh)
	//{
	//	for (auto& chunk : m_chunks)
	//	{
	//		delete chunk.second.chunk;
	//	}
	//	m_chunks.clear();
	//	RenderSettings::Get().deleteMesh = false;
	//}

	int i = 10;
	for (int j = -i; j <= i; j++)
	{
		for (int k = -i; k <= i; k++)
		{
			for (int l = -i; l <= i; l++)
			{
				glm::i32vec3 chunkPos = glm::i32vec3(
					j + position.x / float(CHUNK_UNIT_SIZE),
					k + position.y / float(CHUNK_UNIT_SIZE),
					l + position.z / float(CHUNK_UNIT_SIZE));
				if (!m_chunks[chunkPos])
				{
					Chunk* newChunk = CreateChunk(chunkPos);

					//redundant but w/e
					if (newChunk)
					{
						// notify neighbors
						for (uint m = 0; m < BlockFace::NumFaces; m++)
						{
							NotifyNeighbor(newChunk, chunkPos, BlockFace(m), s_opposingBlockFaces[m]);
							// note to self: reads to neighbor ptrs need to be locked. 
						}
						//batch submit these?
						m_threadPool.Submit(std::bind(&Chunk::GenerateVolume, newChunk), Priority_Med);
					}
				}
			}
		}
	}
}

void VoxelScene::TestUpdate(const glm::vec3& position)
{
	std::unordered_set<Chunk*> generateMeshList;
	std::vector<glm::vec3> poss = {
		glm::vec3(0, -1, 0),
		glm::vec3(1, -1, 0),
		glm::vec3(-1, -1, 0),
		glm::vec3(0, 0, 0),
		glm::vec3(0, -2, 0),
		glm::vec3(0, -1, 1),
		glm::vec3(0, -1, -1),
	};
	m_chunks.clear();
	for (auto pos : poss)
	{
		glm::i32vec3 chunkPos = glm::i32vec3(pos);
		Chunk* newChunk = CreateChunk(chunkPos);

		if (newChunk)
		{
			generateMeshList.emplace(newChunk);

			// notify neighbors
			if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Front])])
			{
				if (neighbor->UpdateNeighborRef(BlockFace::Back, newChunk))
					generateMeshList.emplace(neighbor);
				newChunk->UpdateNeighborRef(BlockFace::Front, neighbor);
			}
			if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Back])])
			{
				if (neighbor->UpdateNeighborRef(BlockFace::Front, newChunk))
					generateMeshList.emplace(neighbor);
				newChunk->UpdateNeighborRef(BlockFace::Back, neighbor);
			}
			if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Right])])
			{
				if (neighbor->UpdateNeighborRef(BlockFace::Left, newChunk))
					generateMeshList.emplace(neighbor);
				newChunk->UpdateNeighborRef(BlockFace::Right, neighbor);
			}
			if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Left])])
			{
				if (neighbor->UpdateNeighborRef(BlockFace::Right, newChunk))
					generateMeshList.emplace(neighbor);
				newChunk->UpdateNeighborRef(BlockFace::Left, neighbor);
			}
			if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Top])])
			{
				if (neighbor->UpdateNeighborRef(BlockFace::Bottom, newChunk))
					generateMeshList.emplace(neighbor);
				newChunk->UpdateNeighborRef(BlockFace::Top, neighbor);
			}
			if (Chunk* neighbor = m_chunks[chunkPos + glm::i32vec3(s_blockNormals[BlockFace::Bottom])])
			{
				if (neighbor->UpdateNeighborRef(BlockFace::Top, newChunk))
					generateMeshList.emplace(neighbor);
				newChunk->UpdateNeighborRef(BlockFace::Bottom, neighbor);
			}
		}
	}
}

void VoxelScene::GenerateMeshes()
{
	//horrible. fix later
	m_GenerateMeshCallbackListMutex.lock();
	while (!m_generateMeshCallbackList.empty())
	{
		m_generateMeshList.push(m_generateMeshCallbackList.front());
		m_generateMeshCallbackList.pop();
	}
	m_GenerateMeshCallbackListMutex.unlock();

	const uint frameMaxMeshes = m_generateMeshList.size();
	const uint iterations = std::min(uint(m_generateMeshList.size()), frameMaxMeshes);
	for (uint i = 0; i < iterations; i++)
	{
		Chunk* chunk = m_generateMeshList.front();
		m_generateMeshList.pop();

		std::unique_lock lock(chunk->m_mutex, std::try_to_lock);
		if (!lock.owns_lock()) {
			continue;
		}

		// this call should be locked since its accessing state. how to deal with this without double locking?
		Chunk::ChunkState chunkState = chunk->GetChunkState();
		if (chunkState == Chunk::ChunkState::WaitingForMeshGeneration)
		{
			if (chunk->AreNeighborsGenerated())
			{
				m_threadPool.Submit(std::bind(&Chunk::GenerateMesh, chunk), Priority_Med);
				continue;
			}
		}

		//otherwise re-add to queue to try again later
		m_generateMeshList.push(chunk);
	}
}

void VoxelScene::Render(const Camera* camera, const Camera* debugCullCamera)
{
	s_shaderProgram.Use();
	glm::mat4 Projection = glm::perspective(camera->GetFovY(), camera->GetAspectRatio(), camera->GetNearClip(), camera->GetFarClip());
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));

	RenderSettings::DrawMode drawMode = RenderSettings::Get().m_drawMode;

	// should probably shove these into a render list
	for (auto& item : m_chunks) 
	{
		Chunk* chunk = item.second;
		const glm::vec3& chunkPos = item.first;

		if (chunk == nullptr)
			continue;

		// might want to figure out a way to do this without locking maybe? idk
		if (!chunk->Renderable())
			continue;

		glm::mat4 Model = glm::translate(glm::mat4(1.0f), glm::vec3(chunkPos) * float(CHUNK_UNIT_SIZE));
		if (chunk->IsInFrustum(debugCullCamera->GetFrustum(), Model))
		{
			glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * Model));
			chunk->Render(drawMode);
		}
	}
}

glm::i32vec3 VoxelScene::ConvertWorldPosToChunkPos(const glm::vec3& worldPos)
{
	return worldPos / float(CHUNK_UNIT_SIZE);
}

void VoxelScene::AddToMeshListCallback(Chunk* chunk)
{
	std::lock_guard lock(m_GenerateMeshCallbackListMutex);
	m_generateMeshCallbackList.push(chunk);
}
