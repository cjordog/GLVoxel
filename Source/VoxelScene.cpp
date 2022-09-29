#include "VoxelScene.h"
#include <glad/glad.h>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include "Camera.h"
#include <math.h>

#ifdef DEBUG
#define NOMINMAX
#include <windows.h>
#endif
#ifdef IMGUI_ENABLED
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif


ShaderProgram VoxelScene::s_shaderProgram;
VoxelScene::ImguiData VoxelScene::s_imguiData;

#ifdef DEBUG
const uint RENDER_DISTANCE = 10;
#else
const uint RENDER_DISTANCE = 15;
#endif

VoxelScene::VoxelScene()
{
	m_chunks = std::unordered_map<glm::i32vec3, Chunk*>();
	m_chunks.reserve(10000);
	// if we can ever have more than one voxel scene move this.
	Chunk::InitShared(
		m_threadPool.GetThreadIDs(),
		std::bind(&VoxelScene::AddToMeshListCallback, this, std::placeholders::_1),
		std::bind(&VoxelScene::AddToRenderListCallback, this, std::placeholders::_1));
}

VoxelScene::~VoxelScene()
{
	Chunk::DeleteShared();
	m_threadPool.ClearJobPool();
	m_threadPool.WaitForAllThreadsFinished();

	for (auto& chunk : m_chunks)
		delete chunk.second;
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
	Chunk* chunk = new Chunk(chunkPos, &m_chunkGenParams);
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
	ZoneScoped;
	if (m_chunkGenParams != m_chunkGenParamsNext)
	{
		m_chunkGenParams = m_chunkGenParamsNext;
		ResetVoxelScene();
	}
	GenerateChunks(position);
	//GenerateMeshes();

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
}

void VoxelScene::GenerateChunks(const glm::vec3& position)
{
	ZoneScoped;
	glm::i32vec3 centerChunkPos = position / float(CHUNK_UNIT_SIZE);

	if (m_threadPool.GetSize() < 10)
	{
		if (m_currentGenerateRadius < RENDER_DISTANCE)
		{
			m_currentGenerateRadius++;
		}
		else
		{
			if (centerChunkPos == m_lastGeneratedChunkPos)
				return;
		}
	}
	else
	{
		return;
	}
	glm::i32vec3 chunkPos = centerChunkPos - glm::i32vec3(m_currentGenerateRadius);
	int i = m_currentGenerateRadius;
	for (int j = -i; j <= i; j++)
	{
		chunkPos.x++;
		chunkPos.y = centerChunkPos.y - m_currentGenerateRadius;
		for (int k = -i; k <= i; k++)
		{
			chunkPos.y++;
			chunkPos.z = centerChunkPos.z - m_currentGenerateRadius;
			for (int l = -i; l <= i; l++)
			{
				chunkPos.z++;
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
	m_lastGeneratedChunkPos = centerChunkPos;
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
	ZoneScoped;
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

void VoxelScene::ResetVoxelScene()
{
	m_threadPool.ClearJobPool();
	m_threadPool.WaitForAllThreadsFinished();
	for (auto& chunk : m_chunks)
		delete chunk.second;
	m_chunks.clear();

	m_generateMeshList.clear();
	m_generateMeshCallbackList.clear();
	m_renderList.clear();
	m_renderCallbackList.clear();

	m_currentGenerateRadius = 3;
	m_lastGenerateRadius = 0;
	m_lastGeneratePos = glm::vec3(0, 0, 0);
	m_lastGeneratedChunkPos = glm::i32vec3(UINT_MAX, UINT_MAX, UINT_MAX);
}

void VoxelScene::Render(const Camera* camera, const Camera* debugCullCamera)
{
	ZoneNamed(SetupRender, true);
	s_shaderProgram.Use();
	glm::mat4 Projection = glm::perspective(camera->GetFovY(), camera->GetAspectRatio(), camera->GetNearClip(), camera->GetFarClip());
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));

	RenderSettings::DrawMode drawMode = RenderSettings::Get().m_drawMode;

	m_renderCallbackListMutex.lock();
	if (m_renderCallbackList.size())
		m_renderList.insert(m_renderList.end(), m_renderCallbackList.begin(), m_renderCallbackList.end());
	m_renderCallbackList.clear();
	m_renderCallbackListMutex.unlock();

	// throw this on a thread? 
	glm::vec3 chunkPos = m_lastGeneratedChunkPos;
	m_renderList.sort([chunkPos](const Chunk* a, const Chunk* b) 
		{ return glm::length2(chunkPos - a->m_chunkPos) < glm::length2(chunkPos - b->m_chunkPos); }
	);
	
	uint vertexCount = 0;
	uint numRenderChunks = 0;
	double totalGenTime = 0;

	glm::mat4 modelMat;
	ZoneNamed(Render, true);
	for (Chunk* chunk : m_renderList)
	{
		if (chunk == nullptr)
			continue;

		//might need to check this again if we ever want to remove an element
		//if (!chunk->Renderable())
		//	continue;

		if (chunk->IsInFrustum(debugCullCamera->GetFrustum(), chunk->m_chunkPos * float(CHUNK_UNIT_SIZE)))
		{
			chunk->GetModelMat(modelMat);
			glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * modelMat));
			vertexCount += chunk->GetVertexCount();
			totalGenTime += chunk->m_genTime;
			numRenderChunks++;
			chunk->Render(drawMode);
		}
	}
	s_imguiData.numTotalChunks = m_chunks.size();
	s_imguiData.numRenderChunks = numRenderChunks;
	s_imguiData.numVerts = vertexCount;
	s_imguiData.avgChunkGenTime = totalGenTime / s_imguiData.numRenderChunks;
}
#ifdef IMGUI_ENABLED
void VoxelScene::RenderImGui()
{
	ImGui::Text("%d vertices", s_imguiData.numVerts);
	ImGui::Text("%d render chunks", s_imguiData.numRenderChunks);
	ImGui::Text("%d total chunks", s_imguiData.numTotalChunks);
	ImGui::Text("%f avg gen time", s_imguiData.avgChunkGenTime);

	ImGui::SliderFloat("cave frequency", &m_chunkGenParamsNext.caveFrequency, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
}
#endif

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
					j + m_lastGeneratedChunkPos.x,
					k + m_lastGeneratedChunkPos.y,
					l + m_lastGeneratedChunkPos.z);
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