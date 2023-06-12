#pragma once

#include <unordered_map>
#include <list>
#include <deque>
#include <mutex>
#include <climits>

#include "glm/vec3.hpp"
#include "Common.h"
#include "Chunk.h"
#include "glm/gtx/hash.hpp"
#include "Octree.h"
#include "ShaderProgram.h"
#include "ThreadPool.h"
#include "Collider.h"
#include "BoxCollider.h"

class Chunk;
class Camera;

class VoxelScene
{
public:
	// #ifdef DEBUG
	struct DebugParams
	{
		bool m_validateThisFrame = false;
		bool m_regenerateWorld = false;
	};

	struct VoxelRayHit
	{
		glm::i8vec3 voxelIndex;
		Chunk* chunk;
		glm::vec3 hitPosition;
		glm::vec3 hitNormal;
		glm::vec3 voxelHitPosition;
	};

	VoxelScene();
	~VoxelScene();

	static void InitShared();

	Chunk* CreateChunk(const glm::i32vec3& chunkPos);
	void Update(const glm::vec3& position);
	void GenerateChunks(const glm::vec3& position);
	void TestUpdate(const glm::vec3& position);
	void GenerateMeshes();
	void ResetVoxelScene();
	void Render(const Camera* camera, const Camera* debugCullCamera);
	void RenderTransparency(const Camera* camera, const Camera* debugCullCamera);
	bool RayCast(const Ray& ray, VoxelRayHit& voxelRayHit);
	bool DeleteBlock(const Ray& ray);
	void ResolveBoxCollider(BoxCollider& collider, float timeDelta);
	void ResolveBoxCollider2(BoxCollider& collider, float timeDelta);
#ifdef IMGUI_ENABLED
	void RenderImGui();
#endif
	inline void NotifyNeighbor(Chunk* chunk, glm::i32vec3 pos, BlockFace side, BlockFace oppositeSide);

	glm::i32vec3 ConvertWorldPosToChunkPos(const glm::vec3& worldPos);

	void AddToMeshListCallback(Chunk* chunk);
	void AddToRenderListCallback(Chunk* chunk);
	void LogVector(const glm::vec3& v);

	struct ImguiData
	{
		uint numVerts;
		uint numRenderChunks;
		uint numTotalChunks;
		double avgChunkGenTime;
	};
	static ImguiData s_imguiData;


private:
	void RenderDebugBoundingBoxes(const Camera* camera, const Camera* debugCullCamera);
	void RenderHitPos(const Camera* camera, const Camera* debugCullCamera);
#ifdef DEBUG
	void ValidateChunks();
#endif

	Octree m_octree;
	std::unordered_map<glm::i32vec3, Chunk*> m_chunks;

	std::deque<Chunk*> m_generateMeshList;
	std::deque<Chunk*> m_generateMeshCallbackList;
	std::list<Chunk*> m_renderList;	// even better, generate buffers on main not in render function so these can be const Chunk*
	std::list<Chunk*> m_renderCallbackList;

	std::mutex m_generateMeshCallbackListMutex;
	std::mutex m_renderCallbackListMutex;

	static ShaderProgram s_chunkShaderProgram;
	static ShaderProgram s_debugWireframeShaderProgram;

	ThreadPool m_threadPool;

	uint m_currentGenerateRadius = 3;
	uint m_lastGenerateRadius = 0;
	glm::vec3 m_lastGeneratePos;
	glm::i32vec3 m_lastGeneratedChunkPos = glm::i32vec3(UINT_MAX, UINT_MAX, UINT_MAX);

	float* m_chunkScratchpadMem;
	Chunk::ChunkGenParams m_chunkGenParams;
	Chunk::ChunkGenParams m_chunkGenParamsNext;

	uint m_chunkVAO = 0;
	uint m_debugWireframeVAO = 0;
	uint m_aabbVBO = 0;
	uint m_aabbEBO = 0;

	std::vector<VertexP> m_aabbVerts;
	std::vector<uint> m_aabbIndices;
	uint m_aabbVertexCount = 0;
	uint m_aabbIndexCount = 0;

	bool m_useOctree = true;
	std::vector<Chunk*> m_frameChunks;
	glm::mat4 m_projMat;

	Chunk::ChunkNoiseGenerators m_noiseGenerators;

	std::mutex testMutex;

	glm::vec3 hitPos = glm::vec3(0);
};