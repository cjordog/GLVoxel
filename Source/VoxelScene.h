#pragma once

#include <unordered_map>

#include "glm/vec3.hpp"
#include "Common.h"
#include "glm/gtx/hash.hpp"
#include "ShaderProgram.h"

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
	void Render(const Camera* camera, const Camera* debugCullCamera);

	glm::i32vec3 ConvertWorldPosToChunkPos(const glm::vec3& worldPos);


private:

	std::unordered_map<glm::i32vec3, Chunk*> m_chunks;


	static ShaderProgram s_shaderProgram;
};