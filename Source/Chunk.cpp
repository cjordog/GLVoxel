#include "Chunk.h"
#include <glad/glad.h>
#include <iostream>
#include "glm/gtc/noise.hpp"
#include <algorithm>
#include "MemPooler.h"
//#include <Tracy.hpp>

float smoothstep(float edge0, float edge1, float x) {
	// Scale, bias and saturate x to 0..1 range
	x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	// Evaluate polynomial
	return x * x * (3 - 2 * x);
}

static std::unordered_map<std::thread::id, int> s_threadIDs;
static float* s_sharedScratchpadMemory;

static std::function<void(Chunk*)> s_generateMeshCallback;
static std::function<void(Chunk*)> s_renderListCallback;

static const Chunk::ChunkGenParams* s_chunkGenParams = nullptr;

// can solve for this inital value
static MemPooler<Chunk::VoxelData> s_memPool(30000);

static std::vector<uint> s_chunkIndices;
uint s_chunkEBO = 0;

Chunk::ScratchpadMemoryLayout* Chunk::s_scratchpadMemory;

Chunk::Chunk()
{
	// can bulk generate these from our voxelscene when we create a bunch of chunks.
	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_EBO);
}

Chunk::Chunk(
	const glm::vec3& chunkPos,
	uint lod
	//const ChunkGenParams* chunkGenParams
)
	: Chunk()
{
	CreateResources(chunkPos, lod);
}

Chunk::~Chunk()
{
	glDeleteBuffers(1, &m_VBO);
	glDeleteBuffers(1, &m_EBO);

	s_memPool.Free(m_voxelData);
}

void Chunk::ReleaseResources()
{
	for (uint i = 0; i < BlockFace::NumFaces; i++)
		m_neighbors[i] = 0;

	m_vertices.clear();
	//m_indices.clear();

	m_vertexCount = 0;
	m_indexCount = 0;

	m_state = ChunkState::BrandNew;
	m_neighborGeneratedMask = 0;
	m_generated = false;
	m_renderable = false;

	m_meshGenerated = 0;
	m_buffersGenerated = 0;
	m_empty = 1;
	m_noGeo = 0;
}

void Chunk::CreateResources(const glm::vec3& chunkPos, uint lod)
{
	m_chunkPos = chunkPos;
	m_LOD = lod;
	m_scale = float(1u << lod);
	glm::vec3 worldSpacePos = chunkPos;
	m_AABB = AABB(worldSpacePos, worldSpacePos + glm::vec3(CHUNK_UNIT_SIZE * m_scale));

	m_modelMat = glm::mat4(1.0f);
	m_modelMat = glm::translate(m_modelMat, m_chunkPos);
	m_modelMat = glm::scale(m_modelMat, glm::vec3(float(m_scale / UNIT_VOXEL_RESOLUTION)));
}

void Chunk::InitShared(
	std::unordered_map<std::thread::id, int>& threadIDs,
	std::function<void(Chunk*)> generateMeshCallback,
	std::function<void(Chunk*)> renderListCallback,
	const ChunkGenParams* chunkGenParams
)
{
	s_threadIDs = threadIDs;

	s_sharedScratchpadMemory = new float[2 * s_threadIDs.size() * INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE];
	s_scratchpadMemory = new ScratchpadMemoryLayout[s_threadIDs.size()];

	s_generateMeshCallback = generateMeshCallback;
	s_renderListCallback = renderListCallback;

	s_chunkGenParams = chunkGenParams;

	//s_memPool = MemPooler<VoxelData>(15000);

	constexpr uint MAX_FACES = 100000;
	uint indexCount = MAX_FACES * 6;
	s_chunkIndices.reserve(indexCount);
	uint vertexCount = 0;
	for (uint i = 0; i < MAX_FACES; i++)
	{
		for (uint j = 0; j < 6; j++)
		{
			s_chunkIndices.push_back(s_indices[j] + vertexCount);
		}
		vertexCount += 4;
	}

	glGenBuffers(1, &s_chunkEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_chunkEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(uint), s_chunkIndices.data(), GL_STATIC_DRAW);
}

void Chunk::DeleteShared()
{
	delete s_sharedScratchpadMemory;
	glDeleteBuffers(1, &s_chunkEBO);
}

inline bool BlockIsOpaque(Chunk::BlockType t)
{
	switch (t)
	{
	case Chunk::BlockType::Dirt:
	case Chunk::BlockType::Grass:
	case Chunk::BlockType::Stone:
		return true;
	default: 
		return false;
	}
}

inline glm::vec3 GetBlockColor(Chunk::BlockType t)
{
	switch (t)
	{
	case Chunk::BlockType::Dirt:
		return { 0.7f, 0.39f, 0.11f };
	case Chunk::BlockType::Grass:
		return { 0.0f, 0.8f, 0.1f };
	case Chunk::BlockType::Stone:
		return { 0.7f, 0.7f, 0.7f };
	default:
		return { 1, 0, 0 };
	}
}

Chunk::BlockType Chunk::GetBlockType(uint x, uint y, uint z) const
{
	if (x >= CHUNK_VOXEL_SIZE || y >= CHUNK_VOXEL_SIZE || z >= CHUNK_VOXEL_SIZE)
		return BlockType::Air;
	return m_voxelData->m_voxels[x + 1][y + 1][z + 1];
}

bool Chunk::VoxelIsCollideable(const glm::i32vec3& index) const
{
	if (m_empty)
		return false;
	const glm::i32vec3 intIndex = index + glm::i32vec3(1);
	switch (m_voxelData->m_voxels[intIndex.x][intIndex.y][intIndex.z])
	{
	case Chunk::BlockType::Dirt:
	case Chunk::BlockType::Grass:
	case Chunk::BlockType::Stone:
		return true;
	default:
		return false;
	}
}

bool Chunk::VoxelIsCollideableAtWorldPos(const glm::vec3& worldPos) const
{
	if (m_empty)
		return false;
	return VoxelIsCollideable((worldPos - m_chunkPos) * float(UNIT_VOXEL_RESOLUTION) / m_scale);
}

Chunk::ChunkState Chunk::GetChunkState() const
{
	return m_state;
}

bool Chunk::GetVoxelIndexAtWorldPos(const glm::vec3& worldPos, glm::i32vec3& voxelIndex)
{
	voxelIndex = (worldPos - m_chunkPos) * float(UNIT_VOXEL_RESOLUTION);
	return true;
}

Chunk::BlockType Chunk::GetBlockTypeAtWorldPos(const glm::vec3& worldPos)
{
	glm::i32vec3 voxelIndex = (worldPos - m_chunkPos) * float(UNIT_VOXEL_RESOLUTION);
	voxelIndex += glm::i32vec3(1);
	return m_voxelData->m_voxels[voxelIndex.x][voxelIndex.y][voxelIndex.z];
}

bool Chunk::ReadyForMeshGeneration() const
{
	return (AllNeighborsGenerated() && m_state == WaitingForMeshGeneration);
}

void Chunk::DeleteBlockAtIndex(const glm::i8vec3& index)
{
	m_voxelData->m_voxels[index.x + 1][index.y + 1][index.z + 1] = BlockType::Air;
}

void Chunk::DeleteBlockAtInternalIndex(const glm::i8vec3& index)
{
	m_voxelData->m_voxels[index.x][index.y][index.z] = BlockType::Air;
}

void Chunk::ReplaceBlockAtIndex(const glm::i8vec3& index, BlockType b)
{
	m_voxelData->m_voxels[index.x + 1][index.y + 1][index.z + 1] = b;
}

bool Chunk::Renderable() const 
{
	return m_renderable;
}

void Chunk::Render(RenderSettings::DrawMode drawMode)
{
	if (!m_meshGenerated)
		return;

	//maybe shouldnt be in render?
	if (!m_buffersGenerated)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, m_vertexCount * sizeof(uint), (uint*)m_vertices.data(), GL_STATIC_DRAW);

		m_vertices.clear();
		m_vertices.resize(0);
		m_buffersGenerated = true;
		m_state = ChunkState::Done;
	}
	glBindVertexBuffer(0, m_VBO, 0, sizeof(uint));
	// this only needs to be bound once
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_chunkEBO);

	glUniformMatrix4fv(0, 1, GL_FALSE, &m_modelMat[0][0]);

	uint dm = (drawMode == RenderSettings::DrawMode::Triangles ? GL_TRIANGLES : GL_LINES);
	glDrawElements(dm, m_indexCount, GL_UNSIGNED_INT, 0);
}

bool Chunk::UpdateNeighborRef(BlockFace face, Chunk* neighbor)
{
	// can we make neighbors atomic too
	//std::unique_lock lock(m_mutex);
	m_neighbors[face] = neighbor;
	if (m_generated)
	{
		//lock.unlock();
		neighbor->NotifyNeighborOfVolumeGeneration(s_opposingBlockFaces[face]);
	}
	return false;
}

bool Chunk::UpdateNeighborRefNewChunk(BlockFace face, Chunk* neighbor)
{
	// dont need to lock here since chunk hasnt been submitted to threadpool at this point. 
	// if we can ever delete a chunk while its being worked on then lock this!
	m_neighbors[face] = neighbor;
	return false;
}

void Chunk::NotifyNeighborOfVolumeGeneration(BlockFace neighbor)
{
	//TODO look into atomic load args for these
	m_neighborGeneratedMask |= 1u << neighbor; // if we unload a chunk, we might not want this to be an |=
	if (AllNeighborsGenerated() && m_state == ChunkState::CollectingNeighborRefs)
	{
		m_state = ChunkState::WaitingForMeshGeneration;
		s_generateMeshCallback(this);
	}
}

void Chunk::GenerateVolume(const ChunkNoiseGenerators* generators)
{
	auto startTime = std::chrono::high_resolution_clock::now();

	m_voxelData = s_memPool.New();
	const float TERRAIN_HEIGHT = s_chunkGenParams->terrainHeight;
	constexpr float DIRT_HEIGHT = 2.0f;
	constexpr float FREQUENCY = 1 / 200.f;

	constexpr int domainTurbulence = 0;// 10 * UNIT_VOXEL_RESOLUTION;

	const int turbulentRowSize = INT_CHUNK_VOXEL_SIZE + domainTurbulence * 2;
	ScratchpadMemoryLayout& scratchMem = s_scratchpadMemory[s_threadIDs[std::this_thread::get_id()]];
	float* noiseOutput2D = scratchMem.noise2D1;
	float* noiseOutput2D2 = scratchMem.noise2D2;
	float* noiseOutput3D = scratchMem.noise3D1;
	float* noiseOutput3D2 = scratchMem.noise3D2;
	if (!s_chunkGenParams->m_debugFlatWorld)
	{
		// samples once per int, so we pass in bigger positions than our actual worldspace position...
		generators->noiseGenerator->GenUniformGrid2D(
			noiseOutput2D,
			((m_chunkPos.x - (1 + domainTurbulence) * m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			((m_chunkPos.z - (1 + domainTurbulence) * m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			turbulentRowSize,
			turbulentRowSize,
			FREQUENCY / UNIT_VOXEL_RESOLUTION * m_scale * s_chunkGenParams->terrainFrequency,
			1
		);
		// this is kinda overkill for just the dirt
		generators->noiseGenerator->GenUniformGrid2D(
			noiseOutput2D2,
			((m_chunkPos.x - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			((m_chunkPos.z - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			INT_CHUNK_VOXEL_SIZE,
			INT_CHUNK_VOXEL_SIZE,
			FREQUENCY / UNIT_VOXEL_RESOLUTION * 20 * m_scale,
			1337
		);
		generators->noiseGenerator->GenUniformGrid3D(
			noiseOutput3D2,
			((m_chunkPos.x - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			((m_chunkPos.y - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			((m_chunkPos.z - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			INT_CHUNK_VOXEL_SIZE,
			INT_CHUNK_VOXEL_SIZE,
			INT_CHUNK_VOXEL_SIZE,
			FREQUENCY / UNIT_VOXEL_RESOLUTION * m_scale,
			1337
		);
		generators->noiseGeneratorCave->GenUniformGrid3D(
			noiseOutput3D,
			((m_chunkPos.x - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			((m_chunkPos.y - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			((m_chunkPos.z - m_scale) * UNIT_VOXEL_RESOLUTION) / m_scale,
			INT_CHUNK_VOXEL_SIZE,
			INT_CHUNK_VOXEL_SIZE,
			INT_CHUNK_VOXEL_SIZE,
			FREQUENCY / UNIT_VOXEL_RESOLUTION * s_chunkGenParams->caveFrequency * m_scale,
			1337
		);
	}

	bool emptyVal = 1;
	// would putting y on the inside be faster? what if y was the inner most index on the 3d array?
	for (int z = 0; z < INT_CHUNK_VOXEL_SIZE; z++)
	{
		for (int y = 0; y < INT_CHUNK_VOXEL_SIZE; y++)
		{
			float worldSpaceHeight = (y - 1) * m_scale * VOXEL_UNIT_SIZE + m_chunkPos.y;
			for (int x = 0; x < INT_CHUNK_VOXEL_SIZE; x++)
			{
				// this can be one level up. move y inwards
				//float noiseVal3D = noiseOutput3D[x + CHUNK_VOXEL_SIZE * y + CHUNK_VOXEL_SIZE * CHUNK_VOXEL_SIZE * z];
				float noiseVal3D = 0;
				int indexOffset = turbulentRowSize * domainTurbulence + domainTurbulence;
				int index = int(x + noiseVal3D * domainTurbulence) + int(z + noiseVal3D * domainTurbulence) * turbulentRowSize + indexOffset;

				float worldSpaceNoiseVal, noiseVal2, noiseVal3, dirtHeight;
				if (!s_chunkGenParams->m_debugFlatWorld)
				{
					worldSpaceNoiseVal = (noiseOutput2D[index] + 1) * 0.5f * TERRAIN_HEIGHT;
					//worldSpaceNoiseVal = smoothstep(-1, 1, noiseOutput[index]) * TERRAIN_HEIGHT;
					noiseVal2 = noiseOutput2D2[x + INT_CHUNK_VOXEL_SIZE * z];
					noiseVal3 = noiseOutput3D[x + INT_CHUNK_VOXEL_SIZE * y + INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE * z];
					dirtHeight = (noiseVal2 + 1.0f) * 0.5f * DIRT_HEIGHT;
				}
				else
				{
					worldSpaceNoiseVal = 0;
					noiseVal2 = 0;
					noiseVal3 = 0;
					dirtHeight = (noiseVal2 + 1.0f) * 0.5f * DIRT_HEIGHT;
				}

				worldSpaceHeight = noiseOutput3D2[x + INT_CHUNK_VOXEL_SIZE * y + INT_CHUNK_VOXEL_SIZE * INT_CHUNK_VOXEL_SIZE * z];
				if (worldSpaceHeight > 0.0f)
				{
					m_voxelData->m_voxels[x][y][z] = BlockType::Air;
				}
				else
				{
					if (noiseVal3 > 0.0f)
					{
						m_voxelData->m_voxels[x][y][z] = BlockType::Air;
					}
					else
					{
						m_voxelData->m_voxels[x][y][z] = BlockType::Grass;

						float depth = (worldSpaceNoiseVal - worldSpaceHeight) * UNIT_VOXEL_RESOLUTION / m_scale;
						if (depth >= 0.0f && depth < 1.0f)
							m_voxelData->m_voxels[x][y][z] = BlockType::Grass;
						else if (depth < dirtHeight + 1)
							m_voxelData->m_voxels[x][y][z] = BlockType::Dirt;
						else
							m_voxelData->m_voxels[x][y][z] = BlockType::Stone;
						emptyVal = 0;
					}
				}
			}
		}
	}	

	m_generated.store(true);
	m_empty = emptyVal;
	// can do the same thing with full chunks.
	if (emptyVal)
	{
		s_memPool.Free(m_voxelData);
		m_voxelData = nullptr;
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> time = endTime - startTime;
	m_genTime += time.count();

	GenerateMesh();
}

void Chunk::GenerateMesh()
{
	auto startTime = std::chrono::high_resolution_clock::now();

	std::unique_lock lock(m_mutex);
	if (IsEmpty())
	{
		m_state = ChunkState::Done;
		return;
	}

	if (!m_generated.load())
	{
		assert(1);
		return;
	}

	m_state = ChunkState::GeneratingMesh;

	m_vertexCount = 0;
	m_indexCount = 0;

	m_vertices.clear();
	m_meshGenerated = 0;
	m_noGeo = 0;

	lock.unlock();

	//if (RenderSettings::Get().greedyMesh)
	{
		GenerateGreedyMeshInt();
	}
	//else
	//{
	//	GenerateMeshInt();
	//}

	lock.lock();
	
	if (m_vertexCount == 0)
	{
		m_noGeo = 1;
		m_state = ChunkState::Done;
	}
	else
	{
		m_state = ChunkState::GeneratingBuffers;
	}

	m_meshGenerated = 1;

	m_renderable = (!IsEmpty() && !IsNoGeo() && (m_state == ChunkState::Done || m_state == ChunkState::GeneratingBuffers));

	lock.unlock();

	if (m_renderable)
		s_renderListCallback(this);

	m_buffersGenerated = false;

	auto endTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> time = endTime - startTime;
	m_genTime += time.count();
}

void Chunk::SetNeedsLODSeam(BlockFace f)
{
	m_needsLODSeam = 1;
	m_LODSeamDir = f;
}

void Chunk::SetTerrainParams(float lacunarity, float gain, int octaves)
{
	//s_noiseGenerator = FastNoise::New<FastNoise::FractalFBm>();
	//auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
	//s_noiseGenerator->SetSource(fnSimplex);
	//s_noiseGenerator->SetLacunarity(lacunarity);
	//s_noiseGenerator->SetGain(gain);
	//s_noiseGenerator->SetOctaveCount(octaves);
	////s_noiseGenerator->SetWeightedStrength()
}

bool Chunk::IsInFrustum(const Frustum& f) const
{
	return m_AABB.IsInFrustumWorldspace(f);
}

void Chunk::GenerateMeshInt()
{
	assert(CHUNK_VOXEL_SIZE < 64);
	std::lock_guard lock(m_mutex);
	for (uint x = 0; x < CHUNK_VOXEL_SIZE; x++)
	{
		for (uint y = 0; y < CHUNK_VOXEL_SIZE; y++)
		{
			for (uint z = 0; z < CHUNK_VOXEL_SIZE; z++)
			{
				BlockType currentBlockType = m_voxelData->m_voxels[x + 1][y + 1][z + 1];
				if (BlockIsOpaque(currentBlockType))
				{
					glm::vec3 offset{ x,y,z };
					for (uint i = 0; i < BlockFace::NumFaces; i++)
					{
						const glm::vec normal = s_blockNormals[i];
						int neighborX = x + normal.x + 1;
						int neighborY = y + normal.y + 1;
						int neighborZ = z + normal.z + 1;

						if (BlockIsOpaque(m_voxelData->m_voxels[neighborX][neighborY][neighborZ]))
							continue;

						if (m_needsLODSeam)
							currentBlockType = BlockType::Dirt;
						// add faces
						for (uint j = 0; j < 4; j++)
						{
							// vertex packing idea from https://www.youtube.com/watch?v=VQuN1RMEr1c
							glm::uvec3 localVertexPos = (s_faces[i][j] + offset);
							uint packedVertex = (0x3F & localVertexPos.x) | (0xFC0 & (localVertexPos.y << 6)) | (0x3F000 & (localVertexPos.z << 12)) | (0x1C0000 & (i << 18)) | (0x1FE00000 & (uint8_t(currentBlockType) << 21));
							m_vertices.push_back(packedVertex);
						}
						m_indexCount += 6;
						m_vertexCount += 4;
						//m_indices.insert(m_indices.end(), std::begin(indices), std::end(indices));
					}
				}
			}
		}
	}
}

// https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
void Chunk::GenerateGreedyMeshInt()
{
	// sweep over each axis, generate forward and backward facing planes in one iteration
	for (int dim = 0; dim < 3; dim++)
	{
		// u,v indices of other two dimensions that were not sweeping
		int u = (dim + 1) % 3;
		int v = (dim + 2) % 3;

		// x is scratch pad for position of the block were currently processing.
		glm::i32vec3 x(0, 0, 0);
		glm::i32vec3 sweepDir(0, 0, 0);
		sweepDir[dim] = 1;

		// mask contains values of current slices neighboring voxel information
		uint16_t mask[CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE] = { 0 };

		// sweep over current dimension
		for (x[dim] = -1; x[dim] < int(CHUNK_VOXEL_SIZE); )
		{
			int n = 0;
			// setup mask
			for (x[v] = 0; x[v] < CHUNK_VOXEL_SIZE; x[v]++)
			{
				for (x[u] = 0; x[u] < CHUNK_VOXEL_SIZE; x[u]++)
				{
					// find block type of current block and the neighbor block in the direction were searching
					BlockType t1 = m_voxelData->m_voxels[x[0] + 1][x[1] + 1][x[2] + 1];
					BlockType t2 = m_voxelData->m_voxels[x[0] + sweepDir[0] + 1][x[1] + sweepDir[1] + 1][x[2] + sweepDir[2] + 1];

					bool o1 = BlockIsOpaque(t1);
					bool o2 = BlockIsOpaque(t2);

					// write 0 if both opaque, 1 if we want face pointing in pos dir, 2 if neg
					mask[x[v]][x[u]] = (o1 == o2) ? 0 : ((o1 && !o2) ? uint16_t(t1) * 2 : uint16_t(t2) * 2 + 1);
				}
			}

			x[dim]++;

			// evaluate mask and generate faces as large as possible
			for (int i = 0; i < CHUNK_VOXEL_SIZE; i++)
			{
				for (int j = 0; j < CHUNK_VOXEL_SIZE; )
				{
					if (int maskVal = mask[i][j])
					{
						// find largest rect of same maskVal
						int w = 1, h = 1;
						while (j + w < CHUNK_VOXEL_SIZE && mask[i][j + w] == maskVal)
						{
							w++;
						}
						bool done = false;
						while (i + h < CHUNK_VOXEL_SIZE && !done)
						{
							for (int k = 0; k < w; k++)
							{
								if (mask[i + h][j + k] != maskVal)
								{
									done = true;
									break;
								}
							}
							if (done)
								break;
							h++;
						}

						// generate face from found rect
						x[u] = j;
						x[v] = i;

						glm::i32vec3 du(0, 0, 0);
						du[u] = w;
						glm::i32vec3 dv(0, 0, 0);
						dv[v] = h;

						const glm::vec3 vertices[2][4] = 
						{
							{
								{x[0],					x[1],					x[2]},
								{x[0] + dv[0],			x[1] + dv[1],			x[2] + dv[2]},
								{x[0] + du[0] + dv[0],	x[1] + du[1] + dv[1],	x[2] + du[2] + dv[2]},
								{x[0] + du[0],			x[1] + du[1],			x[2] + du[2]},
							},
							{
								{x[0],					x[1],					x[2]},
								{x[0] + du[0],			x[1] + du[1],			x[2] + du[2]},
								{x[0] + du[0] + dv[0],	x[1] + du[1] + dv[1],	x[2] + du[2] + dv[2]},
								{x[0] + dv[0],			x[1] + dv[1],			x[2] + dv[2]},
							},
						};

						//// faces with maskVal 1 points in positive normal, 2 is negative, so have both windings
						//static constexpr uint indices[2][6] = 
						//{
						//	{
						//		0, 1, 3,
						//		1, 2, 3
						//	},
						//	{
						//		0, 3, 1,
						//		1, 3, 2
						//	},
						//};

						glm::vec3 normal = sweepDir;
						int normalDir = maskVal % 2 == 1 ? 1 : 0;
						if (normalDir == 1)
							normal = -normal;

						BlockFace b = BlockFace(dim * 2 + (normalDir));
						BlockType blockType = BlockType(maskVal / 2);
						// add faces
						for (uint m = 0; m < 4; m++)
						{
							glm::uvec3 localVertexPos = vertices[normalDir][m];
							uint packedVertex = (0x3F & localVertexPos.x) | (0xFC0 & (localVertexPos.y << 6)) | (0x3F000 & (localVertexPos.z << 12)) | (0x1C0000 & (b << 18)) | (0x1FE00000 & (uint8_t(blockType) << 21));
							m_vertices.push_back(packedVertex);
							//m_vertices.push_back(VertexPCN{ vertices[m], sweepDir, normal });
						}
						for (uint m = 0; m < 6; m++)
						{
							//m_indices.push_back(indices[maskVal - 1][m] + m_vertexCount);
						}
						m_indexCount += 6;
						m_vertexCount += 4;

						//Zero-out mask
						for (int l = 0; l < h; ++l)
						{
							for (int k = 0; k < w; ++k)
							{
								mask[i + l][j + k] = 0;
							}
						}
						//Increment counters and continue
						j += w;
					}
					else
					{
						j++;  
					}
				}
			}
		}
	}
}

int Chunk::ConvertDirToNeighborIndex(const glm::vec3& dir)
{
	// idk if this is faster than just an if statement but its cool
	return glm::dot(glm::abs(dir), glm::vec3(0.5f, 2.5f, 4.5f) - 0.5f * dir);
}
