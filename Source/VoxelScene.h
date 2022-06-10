#pragma once

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>

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
	VoxelScene();

	static void InitShared();

	Chunk* CreateChunk(const glm::i32vec3& chunkPos);
	void Update(const glm::vec3& position);
	void TestUpdate(const glm::vec3& position);
	void GenerateMeshes();
	void Render(const Camera* camera, const Camera* debugCullCamera);
	inline void NotifyNeighbor(Chunk* chunk, glm::i32vec3 pos, BlockFace side, BlockFace oppositeSide);

	glm::i32vec3 ConvertWorldPosToChunkPos(const glm::vec3& worldPos);

private:
	struct ChunkWrapper
	{
		Chunk* chunk;
		std::mutex lock;
	};
	std::unordered_map<glm::i32vec3, ChunkWrapper> m_chunks;
	std::unordered_set<Chunk*> m_generateMeshList;

	static ShaderProgram s_shaderProgram;

	std::thread m_thread;
	std::mutex m_mutex;

	ThreadPool m_threadPool;
};