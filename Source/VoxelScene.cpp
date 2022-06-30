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

#ifdef DEBUG
#define NOMINMAX
#include <windows.h>
#endif

ShaderProgram VoxelScene::s_shaderProgram;

const uint RENDER_DISTANCE = 10;

VoxelScene::VoxelScene()
{
	m_chunks = std::unordered_map<glm::i32vec3, Chunk*>();
}

void VoxelScene::InitShared()
{
	s_shaderProgram = ShaderProgram("terrain.vs.glsl", "terrain.fs.glsl");
	Chunk::InitShared();
}

Chunk* VoxelScene::CreateChunk(const glm::i32vec3& chunkPos)
{
	if (m_chunks[chunkPos])
		return nullptr;

	// TODO:: use smart pointers
	Chunk* chunk = new Chunk(chunkPos);
	chunk->m_generateMeshCallback = std::bind(&VoxelScene::AddToMeshListCallback, this, std::placeholders::_1);
	chunk->m_renderListCallback = std::bind(&VoxelScene::AddToRenderListCallback, this, std::placeholders::_1);
	m_chunks[chunkPos] = chunk;
	
	return chunk;
}

inline void VoxelScene::NotifyNeighbor(Chunk* chunk, glm::i32vec3 pos, BlockFace side, BlockFace oppositeSide)
{
	if (Chunk* neighbor = m_chunks[pos + glm::i32vec3(s_blockNormals[side])])
	{
		// this might be too slow to wait for locks
		neighbor->UpdateNeighborRef(oppositeSide, chunk);
		chunk->UpdateNeighborRefNewChunk(side, neighbor);
	}
}

void VoxelScene::Update(const glm::vec3& position, const DebugParams& debugParams)
{
	GenerateChunks(position);
	GenerateMeshes();

#ifdef DEBUG
	if (debugParams.m_validateThisFrame)
		ValidateChunks();
#endif

	if (!RenderSettings::Get().mtEnabled)
	{
		Job j;
		uint count = 0;
		while (m_threadPool.GetPoolJob(j))
		{
			j.func();
			if (count++ > 100)
				break;
		}
	}
	m_firstFrame = false;
}

void VoxelScene::GenerateChunks(const glm::vec3& position)
{
	glm::i32vec3 centerChunkPos = position / float(CHUNK_UNIT_SIZE);

	// TODO:: can dynamically update this based on load
	if (currentGenerateRadius < RENDER_DISTANCE)
	{
		currentGenerateRadius++;
	}
	else
	{
		if (centerChunkPos == lastGeneratedChunkPos)
			return;
	}

	int i = currentGenerateRadius;
	for (int j = -i; j <= i; j++)
	{
		for (int k = -i; k <= i; k++)
		{
			for (int l = -i; l <= i; l++)
			{
				glm::i32vec3 chunkPos = glm::i32vec3(
					j + centerChunkPos.x,
					k + centerChunkPos.y,
					l + centerChunkPos.z);
				if (!m_chunks[chunkPos])
				{
					Chunk* newChunk = CreateChunk(chunkPos);

					// notify neighbors
					for (uint m = 0; m < BlockFace::NumFaces; m++)
					{
						NotifyNeighbor(newChunk, chunkPos, BlockFace(m), s_opposingBlockFaces[m]);
					}
					//batch submit these?
					m_threadPool.Submit(std::bind(&Chunk::GenerateVolume, newChunk), Priority_Med);
				}
			}
		}
	}
	lastGeneratedChunkPos = centerChunkPos;
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
	m_generateMeshCallbackListMutex.lock();
	if (m_generateMeshCallbackList.size())
		m_generateMeshList.insert(m_generateMeshList.end(), m_generateMeshCallbackList.begin(), m_generateMeshCallbackList.end());
	m_generateMeshCallbackList.clear();
	m_generateMeshCallbackListMutex.unlock();

	const uint frameMaxMeshes = m_generateMeshList.size();
	const uint iterations = std::min(uint(m_generateMeshList.size()), frameMaxMeshes);
	for (uint i = 0; i < iterations; i++)
	{
		Chunk* chunk = m_generateMeshList.front();
		m_generateMeshList.pop_front();

		if (chunk->ReadyForMeshGeneration())
		{
			m_threadPool.Submit(std::bind(&Chunk::GenerateMesh, chunk), Priority_Med);
			continue;
		}

		//otherwise re-add to queue to try again later
		m_generateMeshList.push_back(chunk);
	}
}

void VoxelScene::Render(const Camera* camera, const Camera* debugCullCamera)
{
	s_shaderProgram.Use();
	glm::mat4 Projection = glm::perspective(camera->GetFovY(), camera->GetAspectRatio(), camera->GetNearClip(), camera->GetFarClip());
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));

	RenderSettings::DrawMode drawMode = RenderSettings::Get().m_drawMode;

	m_renderCallbackListMutex.lock();
	if (m_renderCallbackList.size())
		m_renderList.insert(m_renderList.end(), m_renderCallbackList.begin(), m_renderCallbackList.end());
	m_renderCallbackList.clear();
	m_renderCallbackListMutex.unlock();

	for (Chunk* chunk : m_renderList)
	{
		if (chunk == nullptr)
			continue;

		//might need to check this again if we ever want to remove an element
		//if (!chunk->Renderable())
		//	continue;

		glm::mat4 Model = glm::translate(glm::mat4(1.0f), glm::vec3(chunk->m_chunkPos) * float(CHUNK_UNIT_SIZE));
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
	std::lock_guard lock(m_generateMeshCallbackListMutex);
	m_generateMeshCallbackList.push_back(chunk);
}

void VoxelScene::AddToRenderListCallback(Chunk* chunk)
{
	std::lock_guard lock(m_renderCallbackListMutex);
	m_renderCallbackList.push_back(chunk);
}

#ifdef DEBUG
void VoxelScene::ValidateChunks()
{
	int i = RENDER_DISTANCE - 1;
	for (int j = -i; j <= i; j++)
	{
		for (int k = -i; k <= i; k++)
		{
			for (int l = -i; l <= i; l++)
			{
				glm::i32vec3 chunkPos = glm::i32vec3(
					j + lastGeneratedChunkPos.x,
					k + lastGeneratedChunkPos.y,
					l + lastGeneratedChunkPos.z);
				if (Chunk* chunk = m_chunks[chunkPos])
				{
					chunk->m_mutex.lock();
					if (chunk->GetChunkState() < Chunk::ChunkState::GeneratingBuffers)
					{
						char str[128];
						sprintf(str, "Chunk x:%i y:%i z:%i state:%u\n", chunkPos.x, chunkPos.y, chunkPos.z, chunk->GetChunkState());
						OutputDebugString(str);
					}
					chunk->m_mutex.unlock();
				}
			}
		}
	}
}
#endif