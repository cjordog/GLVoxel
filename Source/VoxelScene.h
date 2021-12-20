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

	void CreateChunk(int x, int y, int z);
	void Render(Camera* camera);


private:

	std::unordered_map<glm::i32vec3, Chunk*> m_chunks;


	static ShaderProgram s_shaderProgram;
};