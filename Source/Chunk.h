#pragma once

#include "stdint.h"
#include <vector>

#include "Common.h"
#include "RenderSettings.h"

class Chunk
{
public:
	Chunk(const glm::vec3& chunkPos);

	enum class BlockType : uint8_t
	{
		Air = 0,
		Dirt,
		Grass,
		Stone,
	};

	bool BlockIsOpaque(BlockType t);

	float* GetVertexData() { return (float*)(m_vertices.data()); }
	uint* GetIndexData() { return m_indices.data(); }
	uint GetVertexCount() { return m_vertexCount; }
	uint GetIndexCount() { return m_indexCount; }

	BlockType GetBlockType(uint x, uint y, uint z) const;

	bool IsEmpty() const { return bool(m_empty); }
	bool IsNoGeo() const { return bool(m_noGeo); }

	void Render(RenderSettings::DrawMode drawMode) const;
	bool UpdateNeighborRefs(const Chunk* neighbors[BlockFace::NumFaces]);
	bool UpdateNeighborRef(BlockFace face, const Chunk* neighbor);

	void GenerateMesh();

	const glm::vec3 m_chunkPos;

private:
	void Generate(const glm::vec3& chunkPos);

	void GenerateMeshInt();
	void GenerateGreedyMeshInt();

	// could be a rle instead of 3d array? https://0fps.net/2012/01/14/an-analysis-of-minecraft-like-engines/
	// hard to tell whats better
	BlockType m_voxels[CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE] = { BlockType(0) };
	std::vector<Vertex> m_vertices = std::vector<Vertex>();
	std::vector<uint> m_indices = std::vector<uint>();

	const Chunk* m_neighbors[BlockFace::NumFaces] = { 0 };
	
	uint m_vertexCount = 0;
	uint m_indexCount = 0;

	uint m_VBO = 0;
	uint m_EBO = 0;
	uint m_VAO = 0;

	uint m_generated		: 1 = 0;
	uint m_meshGenerated	: 1 = 0;
	uint m_empty			: 1 = 1;
	uint m_noGeo			: 1 = 0;	// could this be combined with m_empty? probably
};