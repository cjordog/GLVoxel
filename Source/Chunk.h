#pragma once

#include "stdint.h"
#include <vector>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <thread>

#include "Common.h"
#include "RenderSettings.h"
#include <FastNoise/FastNoise.h>

class Chunk
{
public:
	struct ChunkGenParams
	{
		float caveFrequency = 1;

		bool operator!=(const ChunkGenParams& rhs)
		{
			return caveFrequency != rhs.caveFrequency;
		}
	};

public:
	Chunk();
	Chunk(
		const glm::vec3& chunkPos,
		const ChunkGenParams* chunkGenParams
	);
	~Chunk();

	void ReleaseResources();
	void CreateResources(const glm::vec3& chunkPos, uint lod);

	enum class BlockType : uint8_t
	{
		Air = 0,
		Dirt,
		Grass,
		Stone,
	};

	enum ChunkState : uint16_t
	{
		BrandNew,
		GeneratingVolume,
		CollectingNeighborRefs,
		WaitingForMeshGeneration,
		GeneratingMesh,
		GeneratingBuffers,
		Done
	};

	static void InitShared(std::unordered_map<std::thread::id, int>& threadIDs, std::function<void(Chunk*)> generateMeshCallback, std::function<void(Chunk*)> renderListCallback);
	static void DeleteShared();

	//bool BlockIsOpaque(BlockType t);

	void GetModelMat(glm::mat4& mat) { mat = m_modelMat; }
	float* GetVertexData() { return (float*)(m_vertices.data()); }
	uint* GetIndexData() { return m_indices.data(); }
	uint GetVertexCount() { return m_vertexCount; }
	uint GetIndexCount() { return m_indexCount; }

	BlockType GetBlockType(uint x, uint y, uint z) const;
	ChunkState GetChunkState() const;
	bool ReadyForMeshGeneration() const;

	bool IsEmpty() const { return bool(m_empty); }
	bool IsNoGeo() const { return bool(m_noGeo); }
	bool Renderable() const; // can we turn our render chunks back into const Chunk*?

	void Render(RenderSettings::DrawMode drawMode);
	bool UpdateNeighborRefs(const Chunk* neighbors[BlockFace::NumFaces]);
	bool UpdateNeighborRef(BlockFace face, Chunk* neighbor);
	bool UpdateNeighborRefNewChunk(BlockFace face, Chunk* neighbor);
	void NotifyNeighborOfVolumeGeneration(BlockFace neighbor);
	void GenerateVolume();
	void GenerateVolume2();
	void GenerateMesh();

	bool IsInFrustum(const Frustum& f, const glm::vec3& worldPos) const;

	glm::vec3 m_chunkPos = glm::vec3(0, 0, 0);
	uint m_LOD = 0;

	std::mutex m_mutex;

private:

	static const int INT_CHUNK_VOXEL_SIZE = CHUNK_VOXEL_SIZE + 2;

	void GenerateMeshInt();
	void GenerateGreedyMeshInt();

	int ConvertDirToNeighborIndex(const glm::vec3& dir);
	
	inline bool AllNeighborsGenerated() const { return m_neighborGeneratedMask == 0x3f; }

	// could be a rle instead of 3d array? https://0fps.net/2012/01/14/an-analysis-of-minecraft-like-engines/
	// hard to tell whats better
	// 
	// maybe dont need to store the extra edges all the time?
	BlockType m_voxels[INT_CHUNK_VOXEL_SIZE][INT_CHUNK_VOXEL_SIZE][INT_CHUNK_VOXEL_SIZE] = { BlockType(0) };
	//TODO:: call reserve on these with some sane value
	std::vector<Vertex> m_vertices = std::vector<Vertex>();
	std::vector<uint> m_indices = std::vector<uint>();

	// testing with these being atomic. dunno if its better/worse/blegh
	std::atomic<Chunk*> m_neighbors[BlockFace::NumFaces] = { nullptr };

	AABB m_AABB;
	glm::mat4 m_modelMat;
	
	uint m_vertexCount = 0;
	uint m_indexCount = 0;

	uint m_VBO = 0;
	uint m_EBO = 0;
	uint m_VAO = 0;

	std::atomic<ChunkState> m_state = ChunkState::BrandNew;
	std::atomic<uint8_t> m_neighborGeneratedMask = 0;
	std::atomic<bool> m_generated = false;
	std::atomic<bool> m_renderable = false;

	uint m_meshGenerated	: 1 = 0;
	uint m_buffersGenerated : 1 = 0;
	uint m_empty			: 1 = 1;
	uint m_noGeo			: 1 = 0;	// could this be combined with m_empty? probably

	const ChunkGenParams* m_chunkGenParams = nullptr;
};