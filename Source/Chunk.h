#pragma once

#include "stdint.h"
#include <vector>
#include <mutex>
#include <functional>

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

	enum class ChunkState
	{
		BrandNew,
		GeneratingVolume,
		CollectingNeighborRefs,
		WaitingForMeshGeneration,
		GeneratingMesh,
		GeneratingBuffers,
		Done
	};

	bool BlockIsOpaque(BlockType t);

	float* GetVertexData() { return (float*)(m_vertices.data()); }
	uint* GetIndexData() { return m_indices.data(); }
	uint GetVertexCount() { return m_vertexCount; }
	uint GetIndexCount() { return m_indexCount; }

	BlockType GetBlockType(uint x, uint y, uint z) const;
	ChunkState GetChunkState() const;
	bool AreNeighborsGenerated() const;

	bool IsEmpty() const { return bool(m_empty); }
	bool IsNoGeo() const { return bool(m_noGeo); }
	bool Renderable();

	void Render(RenderSettings::DrawMode drawMode);
	bool UpdateNeighborRefs(const Chunk* neighbors[BlockFace::NumFaces]);
	bool UpdateNeighborRef(BlockFace face, Chunk* neighbor);
	void NotifyNeighborOfVolumeGeneration(BlockFace neighbor);

	void GenerateVolume();
	void GenerateMesh();

	bool IsInFrustum(Frustum f, glm::mat4 modelMat) const;

	const glm::vec3 m_chunkPos;

	std::mutex m_mutex;
	std::function<void(Chunk*)> m_generateMeshCallback;

private:

	void GenerateMeshInt();
	void GenerateGreedyMeshInt();

	int ConvertDirToNeighborIndex(const glm::vec3& dir);

	// could be a rle instead of 3d array? https://0fps.net/2012/01/14/an-analysis-of-minecraft-like-engines/
	// hard to tell whats better
	BlockType m_voxels[CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE] = { BlockType(0) };
	std::vector<Vertex> m_vertices = std::vector<Vertex>();
	std::vector<uint> m_indices = std::vector<uint>();

	Chunk* m_neighbors[BlockFace::NumFaces] = { 0 };
	uint8_t m_neighborCollectedMask = 0;
	uint8_t m_neighborGeneratedMask = 0;

	AABB m_AABB;
	
	uint m_vertexCount = 0;
	uint m_indexCount = 0;

	uint m_VBO = 0;
	uint m_EBO = 0;
	uint m_VAO = 0;

	ChunkState m_state = ChunkState::BrandNew;

	uint m_generated		: 1 = 0;
	uint m_meshGenerated	: 1 = 0;
	uint m_buffersGenerated : 1 = 0;
	uint m_empty			: 1 = 1;
	uint m_noGeo			: 1 = 0;	// could this be combined with m_empty? probably
};