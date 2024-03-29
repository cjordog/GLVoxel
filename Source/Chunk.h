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
	static const int INT_CHUNK_VOXEL_SIZE = CHUNK_VOXEL_SIZE + 2;

	struct ChunkGenParams
	{
		float caveFrequency = 1;
		float terrainHeight = 400.0f;
		float terrainLacunarity = 3.0f;
		float terrainGain = 0.4f;
		float terrainFrequency = 0.1f;
		int terrainOctaves = 4;

#ifdef DEBUG
		bool m_debugFlatWorld = true;
#else
		bool m_debugFlatWorld = false;
#endif

		bool operator!=(const ChunkGenParams& rhs)
		{
			return caveFrequency != rhs.caveFrequency || 
				terrainHeight != rhs.terrainHeight ||
				terrainLacunarity != rhs.terrainLacunarity ||
				terrainGain != rhs.terrainGain ||
				terrainFrequency != rhs.terrainFrequency ||
				terrainOctaves != rhs.terrainOctaves ||
				m_debugFlatWorld != rhs.m_debugFlatWorld;
		}
	};

public:
	Chunk();
	Chunk(
		const glm::vec3& chunkPos,
		uint lod
		//const ChunkGenParams* chunkGenParams
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
		Sand,
		Blue,
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

	struct VoxelData
	{
		BlockType m_voxels[INT_CHUNK_VOXEL_SIZE][INT_CHUNK_VOXEL_SIZE][INT_CHUNK_VOXEL_SIZE] = { BlockType(0) };
	};

	struct ChunkNoiseGenerators
	{
		FastNoise::SmartNode<> noiseGenerator; 
		FastNoise::SmartNode<> noiseGeneratorCave;
		FastNoise::SmartNode<> biomeGenerator;
	};

	static void InitShared(
		std::unordered_map<std::thread::id, int>& threadIDs, 
		std::function<void(Chunk*)> generateMeshCallback, 
		std::function<void(Chunk*)> renderListCallback,
		const ChunkGenParams* chunkGenParams
	);
	static void DeleteShared();

	//bool BlockIsOpaque(BlockType t);

	void GetModelMat(glm::mat4& mat) { mat = m_modelMat; }
	//float* GetVertexData() { return (float*)(m_vertices.data()); }
	//uint* GetIndexData() { return m_indices.data(); }
	uint GetVertexCount() { return m_vertexCount; }
	uint GetIndexCount() { return m_indexCount; }
	const AABB& GetBoundingBox() const { return m_AABB; }
	const uint GetLOD() const { return m_LOD; }
	const float GetScale() const { return m_scale; }
	bool IsDeletable() const { return m_state == ChunkState::Done || m_state == ChunkState::GeneratingBuffers; }
	bool IsBrandNew() const { return m_state == ChunkState::BrandNew; }
	bool IsDone() const { return m_state == ChunkState::Done; }
	glm::vec3 GetChunkPos() { return m_chunkPos; }

	BlockType GetBlockType(uint x, uint y, uint z) const;
	bool VoxelIsCollideable(const glm::i32vec3& index) const;
	bool VoxelIsCollideableAtWorldPos(const glm::vec3& worldPos) const;
	ChunkState GetChunkState() const;
	bool GetVoxelIndexAtWorldPos(const glm::vec3& worldPos, glm::i32vec3& voxelIndex);
	BlockType GetBlockTypeAtWorldPos(const glm::vec3& worldPos);
	bool ReadyForMeshGeneration() const;

	void DeleteBlockAtIndex(const glm::i8vec3& index);
	void DeleteBlockAtInternalIndex(const glm::i8vec3& index);
	void ReplaceBlockAtIndex(const glm::i8vec3& index, BlockType b);

	bool IsEmpty() const { return bool(m_empty); }
	bool IsNoGeo() const { return bool(m_noGeo); }
	bool Renderable() const; // can we turn our render chunks back into const Chunk*?

	void Render(RenderSettings::DrawMode drawMode);
	bool UpdateNeighborRefs(const Chunk* neighbors[BlockFace::NumFaces]);
	bool UpdateNeighborRef(BlockFace face, Chunk* neighbor);
	bool UpdateNeighborRefNewChunk(BlockFace face, Chunk* neighbor);
	void NotifyNeighborOfVolumeGeneration(BlockFace neighbor);
	void GetNoiseGenPos(
		const glm::vec3& chunkPos, 
		const glm::vec3& pos,
		const uint lod, 
		glm::ivec3& noisePos, 
		float& frequency);
	void GenerateVolume(const ChunkNoiseGenerators* generators);
	void GenerateVolume2();
	void GenerateMesh();
	void SetNeedsLODSeam(BlockFace f);
	static void SetTerrainParams(float lacunarity, float gain, int octaves);

	bool IsInFrustum(const Frustum& f) const;

	// worldspace chunk pos. bottom most corner
	glm::vec3 m_chunkPos = glm::vec3(0, 0, 0);

	std::mutex m_mutex;

	double m_genTime = 0.0f;

	struct ScratchpadMemoryLayout
	{
		float noise3D1[INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE];
		float noise3D2[INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE];
		float noise2D1[INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE];
		float noise2D2[INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE];
		float noise2D3[INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE];
	};
	static ScratchpadMemoryLayout* s_scratchpadMemory;

private:
	void GenerateMeshInt();
	void GenerateGreedyMeshInt();

	int ConvertDirToNeighborIndex(const glm::vec3& dir);
	
	inline bool AllNeighborsGenerated() const { return m_neighborGeneratedMask == 0x3f; }

	// could be a rle instead of 3d array? https://0fps.net/2012/01/14/an-analysis-of-minecraft-like-engines/
	// hard to tell whats better
	// 
	// maybe dont need to store the extra edges all the time?
	VoxelData* m_voxelData = nullptr;

	//TODO:: call reserve on these with some sane value
	std::vector<uint> m_vertices = std::vector<uint>();
	// is there some way to get rid of this... this is a lot of data that is relatively static
	//std::vector<uint> m_indices = std::vector<uint>();

	// testing with these being atomic. dunno if its better/worse/blegh
	std::atomic<Chunk*> m_neighbors[BlockFace::NumFaces] = { nullptr };

	AABB m_AABB;
	glm::mat4 m_modelMat;
	uint m_LOD = 0;
	float m_scale = 0;

	uint m_vertexCount = 0;
	uint m_indexCount = 0;

	uint m_VBO = 0;
	uint m_EBO = 0;

	std::atomic<ChunkState> m_state = ChunkState::BrandNew;
	std::atomic<uint8_t> m_neighborGeneratedMask = 0;
	std::atomic<bool> m_generated = false;
	std::atomic<bool> m_renderable = false;

	BlockFace m_LODSeamDir = BlockFace::Right;

	uint m_meshGenerated	: 1 = 0;
	uint m_buffersGenerated : 1 = 0;
	uint m_empty			: 1 = 1;
	uint m_noGeo			: 1 = 0;	// could this be combined with m_empty? probably
	uint m_needsLODSeam		: 1 = 0;
};