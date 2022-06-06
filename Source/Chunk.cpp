#include "Chunk.h"
#include <glad/glad.h>
#include <iostream>
#include "glm/gtc/noise.hpp"

Chunk::Chunk(const glm::vec3& chunkPos)
	: m_chunkPos(chunkPos),
	m_AABB(glm::vec3(0, 0, 0), glm::vec3(CHUNK_UNIT_SIZE, CHUNK_UNIT_SIZE, CHUNK_UNIT_SIZE))
{
	Generate(chunkPos);

	//TODO:: need destructors for all this
	// can bulk generate these from our voxelscene when we create a bunch of chunks.
	glGenVertexArrays(1, &m_VAO);
	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_EBO);
}

bool Chunk::BlockIsOpaque(BlockType t)
{
	if (t == BlockType::Air)
		return false;
	return true;
}

static glm::vec3 s_faces[BlockFace::NumFaces][4] =
{
	{	// right
		{1.0f,	1.0f,	0.0f},
		{1.0f,	0.0f,	0.0f},
		{1.0f,	0.0f,	1.0f},
		{1.0f,	1.0f,	1.0f}
	},
	{	// Left
		{0.0f,	1.0f,	1.0f},
		{0.0f,	0.0f,	1.0f},
		{0.0f,	0.0f,	0.0f},
		{0.0f,	1.0f,	0.0f}
	},
	{	// Top
		{1.0f,	1.0f,	0.0f},
		{1.0f,	1.0f,	1.0f},
		{0.0f,	1.0f,	1.0f},
		{0.0f,	1.0f,	0.0f}
	},
	{	// Bottom
		{0.0f,	0.0f,	0.0f},
		{0.0f,	0.0f,	1.0f},
		{1.0f,	0.0f,	1.0f},
		{1.0f,	0.0f,	0.0f}
	},
	{	// Front
		{1.0f,	1.0f,	1.0f},
		{1.0f,  0.0f,	1.0f},
		{0.0f,	0.0f,	1.0f},
		{0.0f,	1.0f,	1.0f}
	},
	{	// Back
		{0.0f,	1.0f,	0.0f},
		{0.0f,	0.0f,	0.0f},
		{1.0f,	0.0f,	0.0f},
		{1.0f,	1.0f,	0.0f}
	},
};

static uint s_indices[] = {
	0, 3, 1,
	1, 3, 2
};

Chunk::BlockType Chunk::GetBlockType(uint x, uint y, uint z) const
{
	if (x >= CHUNK_VOXEL_SIZE || y >= CHUNK_VOXEL_SIZE || z >= CHUNK_VOXEL_SIZE)
		return BlockType::Air;
	return m_voxels[x][y][z];
}

void Chunk::Render(RenderSettings::DrawMode drawMode) const
{
	if (!m_meshGenerated)
		return;

	//maybe shouldnt be in render?
	if (!m_buffersGenerated)
	{
		glBindVertexArray(m_VAO);

		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, m_vertexCount * sizeof(Vertex), (float*)m_vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexCount * sizeof(uint), m_indices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(glm::vec3)));
		glEnableVertexAttribArray(2);
	}

	glBindVertexArray(m_VAO);

	uint dm = (drawMode == RenderSettings::DrawMode::Triangles ? GL_TRIANGLES : GL_LINES);
	glDrawElements(dm, m_indexCount, GL_UNSIGNED_INT, 0);
}

bool Chunk::UpdateNeighborRefs(const Chunk* neighbors[BlockFace::NumFaces])
{
	bool updated = false;

	for (uint i = 0; i < BlockFace::NumFaces; i++)
	{
		if (neighbors[i] != m_neighbors[i])
		{
			updated = true;
			m_neighbors[i] = neighbors[i];
		}
	}

	return updated;
}

bool Chunk::UpdateNeighborRef(BlockFace face, const Chunk* neighbor)
{
	if (m_neighbors[face] != neighbor)
	{
		m_neighbors[face] = neighbor;
		return true;
	}
	return false;
}

void Chunk::Generate(const glm::vec3& chunkPos)
{
	for (uint x = 0; x < CHUNK_VOXEL_SIZE; x++)
	{
		for (uint y = 0; y < CHUNK_VOXEL_SIZE; y++)
		{
			for (uint z = 0; z < CHUNK_VOXEL_SIZE; z++)
			{
				if (y / float(UNIT_VOXEL_RESOLUTION) + chunkPos.y * float(CHUNK_UNIT_SIZE) >
					glm::perlin((glm::vec2(chunkPos.x, chunkPos.z) * float(CHUNK_UNIT_SIZE) + glm::vec2(x, z) / float(UNIT_VOXEL_RESOLUTION)) / 57.f) * 20.f)
				{
					m_voxels[x][y][z] = BlockType::Air;
				}
				else 
				{
					m_voxels[x][y][z] = BlockType::Dirt;
					m_empty = 0;
				}
			}
		}
	}
	m_generated = 1;
}

void Chunk::GenerateMesh()
{
	if (IsEmpty() || !m_generated)
		return;

	m_vertexCount = 0;
	m_indexCount = 0;

	m_vertices.clear();
	m_indices.clear();

	if (RenderSettings::Get().greedyMesh)
	{
		GenerateGreedyMeshInt();
	}
	else
	{
		GenerateMeshInt();
	}
	
	if (m_vertexCount == 0)
	{
		m_noGeo = 1;
	}
	else
	{

	}

	m_meshGenerated = 1;
}

bool Chunk::IsInFrustum(Frustum f, glm::mat4 modelMat) const
{
	return m_AABB.IsInFrustum(f, modelMat);
}

void Chunk::GenerateMeshInt()
{
	for (uint x = 0; x < CHUNK_VOXEL_SIZE; x++)
	{
		for (uint y = 0; y < CHUNK_VOXEL_SIZE; y++)
		{
			for (uint z = 0; z < CHUNK_VOXEL_SIZE; z++)
			{
				if (m_voxels[x][y][z] == BlockType::Dirt)
				{
					glm::vec3 offset{ x,y,z };
					for (uint i = 0; i < BlockFace::NumFaces; i++)
					{
						const glm::vec normal = s_blockNormals[i];
						int neighborX = x + normal.x;
						int neighborY = y + normal.y;
						int neighborZ = z + normal.z;

						// if we're not checking an edge face
						if (!(neighborX < 0 || neighborX >= CHUNK_VOXEL_SIZE ||
							neighborY < 0 || neighborY >= CHUNK_VOXEL_SIZE ||
							neighborZ < 0 || neighborZ >= CHUNK_VOXEL_SIZE))
						{
							if (m_voxels[neighborX][neighborY][neighborZ] == BlockType::Dirt)
								continue;
						}
						else // were on the edge face of the chunk
						{
							// compare with neighboring chunk, if it exists
							if (const Chunk* neighborChunk = m_neighbors[i])
							{
								//if (i == 0)
								if (neighborChunk->m_generated &&
									!neighborChunk->IsEmpty())
								{
									if (normal.x)
										neighborX += (neighborX < 0) ? CHUNK_VOXEL_SIZE : -CHUNK_VOXEL_SIZE;
									if (normal.y)
										neighborY += (neighborY < 0) ? CHUNK_VOXEL_SIZE : -CHUNK_VOXEL_SIZE;
									if (normal.z)
										neighborZ += (neighborZ < 0) ? CHUNK_VOXEL_SIZE : -CHUNK_VOXEL_SIZE;

									if (neighborChunk->GetBlockType(neighborX, neighborY, neighborZ) == BlockType::Dirt)
										continue;
								}
							}
						}

						// add faces
						for (uint j = 0; j < 4; j++)
						{
							m_vertices.push_back(Vertex{ s_faces[i][j] + offset, glm::vec3(1.0f, 0.0f, 0.0f), s_blockNormals[i] });
						}
						for (uint j = 0; j < 6; j++)
						{
							m_indices.push_back(s_indices[j] + m_vertexCount);
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

// adopted from https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
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
		uint8_t mask[CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE] = { 0 };

		// sweep over current dimension
		for (x[dim] = -1; x[dim] < int(CHUNK_VOXEL_SIZE); )
		{
			const Chunk* neighborChunk;
			if (x[dim] == -1)
				neighborChunk = m_neighbors[ConvertDirToNeighborIndex(-sweepDir)];
			else if (x[dim] == CHUNK_VOXEL_SIZE - 1)
				neighborChunk = m_neighbors[ConvertDirToNeighborIndex(sweepDir)];

			int n = 0;
			// setup mask
			for (x[v] = 0; x[v] < CHUNK_VOXEL_SIZE; x[v]++)
			{
				for (x[u] = 0; x[u] < CHUNK_VOXEL_SIZE; x[u]++)
				{
					// find block type of current block and the neighbor block in the direction were searching
					BlockType t1 = BlockType::Air;
					BlockType t2 = BlockType::Air;
					if (x[dim] >= 0)
					{
						t1 = m_voxels[x[0]][x[1]][x[2]];
					}
					else
					{
						glm::i32vec3 x2(x);
						x2[dim] += CHUNK_VOXEL_SIZE;
						if (neighborChunk && 
							neighborChunk->m_generated &&
							!neighborChunk->IsEmpty())
						{
							t1 = neighborChunk->GetBlockType(x2.x, x2.y, x2.z);
						}
					}
					if (x[dim] < int(CHUNK_VOXEL_SIZE - 1))
					{
						t2 = m_voxels[x[0] + sweepDir[0]][x[1] + sweepDir[1]][x[2] + sweepDir[2]];
					}
					else
					{
						glm::i32vec3 x2(x);
						x2[dim] -= int(CHUNK_VOXEL_SIZE);
						if (neighborChunk &&
							neighborChunk->m_generated &&
							!neighborChunk->IsEmpty())
						{
							t1 = neighborChunk->GetBlockType(x2.x, x2.y, x2.z);
						}
					}

					bool o1 = BlockIsOpaque(t1);
					bool o2 = BlockIsOpaque(t2);

					// write 0 if both opaque, 1 if we want face pointing in pos dir, 2 if neg
					mask[x[v]][x[u]] = (o1 == o2) ? 0 : ((o1 && !o2) ? 1 : 2);
				}
			}

			x[dim]++;

			// evaluate mask and generate faces as large as possible
			for (int i = 0; i < CHUNK_VOXEL_SIZE; i++)
			{
				for (int j = 0; j < CHUNK_VOXEL_SIZE; )
				{
					//TODO:: check block type (maybe incorporate into mask)
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

						glm::vec3 vertices[4] = 
						{
							{x[0],					x[1],					x[2]},
							{x[0] + du[0],			x[1] + du[1],			x[2] + du[2]},
							{x[0] + du[0] + dv[0],	x[1] + du[1] + dv[1],	x[2] + du[2] + dv[2]},
							{x[0] + dv[0],			x[1] + dv[1],			x[2] + dv[2]},
						};

						// faces with maskVal 1 points in positive normal, 2 is negative, so have both windings
						static constexpr uint indices[2][6] = 
						{
							{
								0, 1, 3,
								1, 2, 3
							},
							{
								0, 3, 1,
								1, 3, 2
							},
						};

						glm::vec3 normal = sweepDir;
						if (maskVal == 2)
							normal = -normal;

						// add faces
						for (uint m = 0; m < 4; m++)
						{
							m_vertices.push_back(Vertex{ vertices[m], sweepDir, normal });
						}
						for (uint m = 0; m < 6; m++)
						{
							m_indices.push_back(indices[maskVal - 1][m] + m_vertexCount);
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
