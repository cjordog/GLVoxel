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


ShaderProgram VoxelScene::s_chunkShaderProgram;
ShaderProgram VoxelScene::s_debugWireframeShaderProgram;
VoxelScene::ImguiData VoxelScene::s_imguiData;

#ifdef DEBUG
const uint RENDER_DISTANCE = 10;
#else
const uint RENDER_DISTANCE = 15;
#endif

VoxelScene::VoxelScene()
{
	m_chunks = std::unordered_map<glm::i32vec3, Chunk*>();

	m_noiseGenerator = FastNoise::New<FastNoise::FractalFBm>();
	auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
	m_noiseGenerator->SetSource(fnSimplex);
	m_noiseGenerator->SetGain(0.1f);
	m_noiseGenerator->SetLacunarity(10);
	m_noiseGenerator->SetOctaveCount(3);

	m_noiseGeneratorCave = FastNoise::New<FastNoise::FractalRidged>();
	auto fnSimplex2 = FastNoise::New<FastNoise::Simplex>();
	m_noiseGeneratorCave->SetSource(fnSimplex2);
	m_noiseGeneratorCave->SetOctaveCount(2);

	// if we can ever have more than one voxel scene move this.
	Chunk::InitShared(
		m_threadPool.GetThreadIDs(),
		std::bind(&VoxelScene::AddToMeshListCallback, this, std::placeholders::_1),
		std::bind(&VoxelScene::AddToRenderListCallback, this, std::placeholders::_1),
		&m_chunkGenParams,
		&m_noiseGenerator,
		&m_noiseGeneratorCave
	);

	glGenVertexArrays(1, &m_chunkVAO);
	//during initialization
	glBindVertexArray(m_chunkVAO);

	//https://riptutorial.com/opengl/example/28662/version-4-3
	constexpr int vertexBindingPoint = 0;// free to choose, must be less than the GL_MAX_VERTEX_ATTRIB_BINDINGS limit

	glVertexAttribIFormat(0, 1, GL_UNSIGNED_INT, 0);
	// set the details of a single attribute
	glVertexAttribBinding(0, vertexBindingPoint);
	// which buffer binding point it is attached to
	glEnableVertexAttribArray(0);

	//glVertexAttribFormat(1, 3, GL_FLOAT, false, offsetof(VertexPCN, color));
	//glVertexAttribBinding(1, vertexBindingPoint);
	//glEnableVertexAttribArray(1);

	//glVertexAttribFormat(2, 3, GL_FLOAT, false, offsetof(VertexPCN, normal));
	//glVertexAttribBinding(2, vertexBindingPoint);
	//glEnableVertexAttribArray(2);

	glGenBuffers(1, &m_aabbVBO);
	glGenBuffers(1, &m_aabbEBO);

	glGenVertexArrays(1, &m_debugWireframeVAO);
	glBindVertexArray(m_debugWireframeVAO);
	glVertexAttribFormat(0, 3, GL_FLOAT, false, offsetof(VertexP, position));
	glVertexAttribBinding(0, vertexBindingPoint);
	glEnableVertexAttribArray(0);
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
	s_chunkShaderProgram = ShaderProgram("terrain.vs.glsl", "terrain.fs.glsl");
	s_debugWireframeShaderProgram = ShaderProgram("DebugWireframe.vs.glsl", "DebugWireframe.fs.glsl");
}

Chunk* VoxelScene::CreateChunk(const glm::i32vec3& chunkPos)
{
	if (m_chunks[chunkPos])
		return nullptr;

	// TODO:: use smart pointers
	Chunk* chunk = new Chunk(chunkPos, 0/*, &m_chunkGenParams*/);
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

void VoxelScene::Update(const glm::vec3& position)
{
	ZoneScoped;
	if (m_chunkGenParams != m_chunkGenParamsNext)
	{
		m_chunkGenParams = m_chunkGenParamsNext;
		ResetVoxelScene();
		m_noiseGenerator->SetLacunarity(m_chunkGenParamsNext.terrainLacunarity);
		m_noiseGenerator->SetGain(m_chunkGenParamsNext.terrainGain);
		m_noiseGenerator->SetOctaveCount(m_chunkGenParamsNext.terrainOctaves);
	}
	m_frameChunks.clear();
	std::vector<Chunk*> newChunks;
	m_octree.GenerateFromPosition(position, newChunks, m_frameChunks);
	for (Chunk* chunk : newChunks)
	{
		m_threadPool.Submit(std::bind(&Chunk::GenerateVolume, chunk, &m_noiseGenerator, &m_noiseGeneratorCave), chunk->GetLOD() == 0 ? Priority_High : Priority_Med);
	}

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

	if (RenderSettings::Get().renderDebugWireframes)
	{
		GatherBoundingBoxes();
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

	m_octree.Clear();
}

void VoxelScene::Render(const Camera* camera, const Camera* debugCullCamera)
{
	ZoneNamed(SetupRender, true);
	s_chunkShaderProgram.Use();
	m_projMat = glm::perspective(camera->GetFovY(), camera->GetAspectRatio(), camera->GetNearClip(), camera->GetFarClip());
	glUniformMatrix4fv(2, 1, GL_FALSE, &m_projMat[0][0]);
	glUniformMatrix4fv(1, 1, GL_FALSE, &camera->GetViewMatrix()[0][0]);
	glUniform3fv(50, 1, &camera->GetPosition()[0]);

	RenderSettings::DrawMode drawMode = RenderSettings::Get().m_drawMode;

	if (!m_useOctree)
	{
		m_renderCallbackListMutex.lock();
		if (m_renderCallbackList.size())
			m_renderList.insert(m_renderList.end(), m_renderCallbackList.begin(), m_renderCallbackList.end());
		m_renderCallbackList.clear();
		m_renderCallbackListMutex.unlock();

		// throw this on a thread? should only sort culled chunks or just not sort at all.
		glm::vec3 chunkPos = m_lastGeneratedChunkPos;
		m_renderList.sort([chunkPos](const Chunk* a, const Chunk* b)
			{ return glm::length2(chunkPos - a->m_chunkPos) < glm::length2(chunkPos - b->m_chunkPos); }
		);
	}

	
	uint vertexCount = 0;
	uint numRenderChunks = 0;
	double totalGenTime = 0;

	ZoneNamed(Render, true);

	glm::mat4 modelMat;
	glBindVertexArray(m_chunkVAO);
	glDepthMask(GL_TRUE);
	
	for (Chunk* chunk : m_frameChunks)
	{
		if (chunk == nullptr || !chunk->Renderable())
			continue;

		if (chunk->IsInFrustum(debugCullCamera->GetFrustum()))
		{
			vertexCount += chunk->GetVertexCount();
			totalGenTime += chunk->m_genTime;
			numRenderChunks++;
			chunk->Render(drawMode);
		}
	}
	s_imguiData.numTotalChunks = m_frameChunks.size();
	s_imguiData.numRenderChunks = numRenderChunks;
	s_imguiData.numVerts = vertexCount;
	s_imguiData.avgChunkGenTime = totalGenTime / s_imguiData.numRenderChunks;

	if (RenderSettings::Get().renderDebugWireframes)
	{
		RenderDebugBoundingBoxes(camera, debugCullCamera);
	}
}

// since velocity * frameDelta buffers are so small, could we not just look if vector goes into new block. 
// if it does, check that block. resolve vectors easily. small edge cases when crossing multiple borders. could then fallback to amanatides and woo 
// but that would almost never happen with these vector sizes? maybe lower framerates it could
void VoxelScene::ResolveCollider(Collider& collider, float timeDelta)
{
	// just do this terribleness for now. i dont want to deal with physics rn
	glm::vec3 worldPos = collider.GetPosition();

	// need to solve edge case if outside the octree
	Chunk* currChunk = m_octree.GetChunkAtWorldPos(worldPos);
	if (currChunk == nullptr || !currChunk->IsDeletable() || currChunk->GetLOD() != 0)
		return;

	glm::vec3 movementDir = collider.m_desiredPosition - worldPos;

	glm::vec3 velocity = collider.GetVelocity();
	velocity = velocity + glm::vec3(0, -10, 0) * (timeDelta / 1000.f);
	collider.SetVelocity(velocity);
	glm::vec3 targetDir = velocity * (timeDelta / 1000.f) + movementDir;

	glm::i32vec3 voxelIndex;
	bool ret = currChunk->GetVoxelIndexAtWorldPos(worldPos, voxelIndex);
	const int stepX = targetDir.x > 0 ? 1 : (targetDir.x < 0 ? -1 : 0);
	const int stepY = targetDir.y > 0 ? 1 : (targetDir.y < 0 ? -1 : 0);
	const int stepZ = targetDir.z > 0 ? 1 : (targetDir.z < 0 ? -1 : 0);
	const glm::i32vec3 step(stepX, stepY, stepZ);

	glm::vec3 chunkPos = currChunk->GetChunkPos();

	const float tDeltaX = VOXEL_UNIT_SIZE / std::max(std::abs(targetDir.x), 0.000001f);
	const float tDeltaY = VOXEL_UNIT_SIZE / std::max(std::abs(targetDir.y), 0.000001f);
	const float tDeltaZ = VOXEL_UNIT_SIZE / std::max(std::abs(targetDir.z), 0.000001f);

	glm::vec3 voxelBorder = glm::vec3(voxelIndex + glm::max(step, 0)) * VOXEL_UNIT_SIZE;
	glm::vec3 tMax = (voxelBorder - (worldPos - chunkPos));
	tMax.x = targetDir.x != 0.0f ? tMax.x / targetDir.x : 1000000.f;
	tMax.y = targetDir.y != 0.0f ? tMax.y / targetDir.y : 1000000.f;
	tMax.z = targetDir.z != 0.0f ? tMax.z / targetDir.z : 1000000.f;

	bool exitedChunk = false;
	int exitIndex = 0;
	float lastT = 0;
	bool collided = false;
	while (!(collided = currChunk->VoxelIsCollideable(voxelIndex)) && lastT < 1.0f)
	{
		if (tMax.x < tMax.y)
		{
			if (tMax.x < tMax.z)
			{
				voxelIndex.x += stepX;
				exitedChunk = (voxelIndex.x > CHUNK_VOXEL_SIZE || voxelIndex.x < 0);
				lastT = tMax.x;
				exitIndex = 0;
				tMax.x += tDeltaX;
			}
			else
			{
				voxelIndex.z += stepZ;
				exitedChunk = (voxelIndex.z > CHUNK_VOXEL_SIZE || voxelIndex.z < 0);
				lastT = tMax.z;
				exitIndex = 2;
				tMax.z += tDeltaZ;

			}
		}
		else 
		{
			if (tMax.y < tMax.z)
			{
				voxelIndex.y += stepY;
				exitedChunk = (voxelIndex.y > CHUNK_VOXEL_SIZE || voxelIndex.y < 0);
				lastT = tMax.y;
				exitIndex = 1;
				tMax.y += tDeltaY;
			}
			else 
			{
				voxelIndex.z += stepZ;
				exitedChunk = (voxelIndex.z > CHUNK_VOXEL_SIZE || voxelIndex.z < 0);
				lastT = tMax.z;
				exitIndex = 2;
				tMax.z += tDeltaZ;
			}
		}
		if (exitedChunk)
		{
			// bump a little in that direction to make sure were in that new chunk and not right on the edge
			glm::vec3 currPos = worldPos + targetDir * (lastT + 0.001f);
			currChunk = m_octree.GetChunkAtWorldPos(currPos);
			currChunk->GetVoxelIndexAtWorldPos(currPos, voxelIndex);
			exitedChunk = false;
		}
	}

	collider.Translate(targetDir * std::min(1.0f, lastT));
	if (lastT < 1.0f) 
	{
		velocity[exitIndex] = 0.0f;
		collider.SetVelocity(velocity);
	}

	if (collided && exitIndex == 1)
	{
		 collider.isGrounded = true;
	}
}

void VoxelScene::FillBoundingBoxBuffer(const AABB& aabb)
{
	glm::vec3 center = aabb.center;
	for (uint i = 0; i < BlockFace::NumFaces; i++)
	{
		for (uint j = 0; j < 4; j++)
		{
			// extents are half extents...
			m_aabbVerts.push_back(VertexP{(s_centeredFaces[i][j] * aabb.extents + center)});
		}
		for (uint j = 0; j < 6; j++)
		{
			m_aabbIndices.push_back(s_indices[j] + m_aabbVertexCount);
		}
		m_aabbIndexCount += 6;
		m_aabbVertexCount += 4;
	}
}

void VoxelScene::GatherBoundingBoxes()
{
	ZoneScoped;
	m_aabbIndexCount = 0;
	m_aabbVertexCount = 0;
	m_aabbVerts.clear();
	m_aabbIndices.clear();
	//for (const auto& chunk : m_renderList)
	for (const Chunk* chunk : m_frameChunks)
	{
		if (!chunk->IsEmpty())
			FillBoundingBoxBuffer(chunk->GetBoundingBox());
	}
}


// TODO:: turn this into an indexed draw on single cube mesh
void VoxelScene::RenderDebugBoundingBoxes(const Camera* camera, const Camera* debugCullCamera)
{
	s_debugWireframeShaderProgram.Use();
	glDepthMask(GL_FALSE);
	glBindVertexArray(m_debugWireframeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_aabbVBO);
	glBufferData(GL_ARRAY_BUFFER, m_aabbVertexCount * sizeof(VertexP), (float*)m_aabbVerts.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_aabbEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_aabbIndexCount * sizeof(uint), m_aabbIndices.data(), GL_DYNAMIC_DRAW);

	glBindVertexBuffer(0, m_aabbVBO, 0, sizeof(VertexP));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_aabbEBO);

	glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * glm::mat4(1)));
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(m_projMat));
	glDrawElements(GL_LINES, m_aabbIndexCount, GL_UNSIGNED_INT, 0);
	glDepthMask(GL_TRUE);
}

#ifdef IMGUI_ENABLED
void VoxelScene::RenderImGui()
{
	ImGui::Text("%d vertices", s_imguiData.numVerts);
	ImGui::Text("%d render chunks", s_imguiData.numRenderChunks);
	ImGui::Text("%d total chunks", s_imguiData.numTotalChunks);
	ImGui::Text("%f avg gen time", s_imguiData.avgChunkGenTime);

	ImGui::SliderFloat("cave frequency", &m_chunkGenParamsNext.caveFrequency, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	//float terrainHeight = 100.0f;
	//float terrainLacunarity = 1.0f;
	//float terrainGain = 1.0f;
	//float terrainFrequency = 1.0f;
	//int terrainOctaves = 4;
	ImGui::SliderFloat("Terrain Height", &m_chunkGenParamsNext.terrainHeight, 1.f, 2000.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Terrain Lacunarity", &m_chunkGenParamsNext.terrainLacunarity, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Terrain Gain", &m_chunkGenParamsNext.terrainGain, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Terrain Frequency", &m_chunkGenParamsNext.terrainFrequency, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::InputInt("Terrain Octaves", &m_chunkGenParamsNext.terrainOctaves, 1, 10);
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