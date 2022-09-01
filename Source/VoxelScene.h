#pragma once

#include <unordered_map>
#include <list>
#include <deque>
#include <mutex>
#include <climits>

#include "glm/vec3.hpp"
#include "Common.h"
#include "glm/gtx/hash.hpp"
#include "ShaderProgram.h"
#include "ThreadPool.h"

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

	VoxelScene();
	~VoxelScene();

	static void InitShared();

	Chunk* CreateChunk(const glm::i32vec3& chunkPos);
	void Update(const glm::vec3& position, const DebugParams& debugParams);
	void GenerateChunks(const glm::vec3& position);
	void TestUpdate(const glm::vec3& position);
	void GenerateMeshes();
	void Render(const Camera* camera, const Camera* debugCullCamera);
	inline void NotifyNeighbor(Chunk* chunk, glm::i32vec3 pos, BlockFace side, BlockFace oppositeSide);

	glm::i32vec3 ConvertWorldPosToChunkPos(const glm::vec3& worldPos);

	void AddToMeshListCallback(Chunk* chunk);
	void AddToRenderListCallback(Chunk* chunk);

	static uint s_numVerts;


private:
#ifdef DEBUG
	void ValidateChunks();
#endif

	std::unordered_map<glm::i32vec3, Chunk*> m_chunks;
	std::deque<Chunk*> m_generateMeshList;
	std::deque<Chunk*> m_generateMeshCallbackList;
	std::list<Chunk*> m_renderList;	// even better, generate buffers on main not in render function so these can be const Chunk*
	std::list<Chunk*> m_renderCallbackList;

	std::mutex m_generateMeshCallbackListMutex;
	std::mutex m_renderCallbackListMutex;

	static ShaderProgram s_shaderProgram;


	ThreadPool m_threadPool;

	uint currentGenerateRadius = 3;
	uint lastGenerateRadius = 0;
	glm::vec3 lastGeneratePos;
	glm::i32vec3 lastGeneratedChunkPos = glm::i32vec3(UINT_MAX, UINT_MAX, UINT_MAX);

	bool m_firstFrame = true;

	float* m_chunkScratchpadMem;
};