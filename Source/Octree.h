#pragma once

#include "Common.h"
// this might not have to be here if i do it right
#include "Chunk.h"

#include "MemPooler.h"
#include <memory>
#include <glm/vec3.hpp>

// we could create an OctreeNode pool here
struct OctreeNode
{
	OctreeNode(Chunk* chunk, const glm::vec3& centerPos, uint lod);
	OctreeNode(const glm::vec3& centerPos, uint lod);

	Chunk* m_chunk = nullptr;
	glm::vec3 m_centerPos = glm::vec3(0);
	uint m_lod = 0;
	std::shared_ptr<OctreeNode> m_children[8] = {nullptr};
};

class Octree
{
public:
	Octree();
	void GenerateFromPosition(glm::vec3 position, std::vector<Chunk*>& newChunks, std::vector<Chunk*>& leafChunks);
	void GenerateFromPosition2(glm::vec3 position, std::vector<Chunk*>& newChunks, std::vector<Chunk*>& leafChunks);
	void Clear();

private:
	std::shared_ptr<OctreeNode> m_root = nullptr;
	int m_size = 0;		// size in chunks
	int m_maxDepth = 0;
	glm::vec3 m_centerPos = glm::vec3(0);
	MemPooler<Chunk> m_memPool;

	//static inline int GetChildIndex(const glm::vec3& octreePos);

	void CreateChildren(std::shared_ptr<OctreeNode> parent);
	bool ReleaseChildren(std::shared_ptr<OctreeNode> node);
	bool ReleaseChildrenBlocking(std::shared_ptr<OctreeNode> node);
};