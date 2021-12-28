#include "Chunk.h"
#include <glad/glad.h>
#include <iostream>
#include "glm/gtc/noise.hpp"

Chunk::Chunk(const glm::vec3& chunkPos)
	: m_chunkPos(chunkPos)
{
	Generate(chunkPos);

	// can bulk generate these from our voxelscene when we create a bunch of chunks.
	glGenVertexArrays(1, &m_VAO);
	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_EBO);
}

static glm::vec3 s_faces[BlockFace::NumFaces][4] =
{
	{	// Front
		{0.5f,	0.5f,	0.5f},
		{0.5f,  -0.5f,	0.5f},
		{-0.5f, -0.5f,	0.5f},
		{-0.5f, 0.5f,	0.5f}
	},
	{	// Back
		{-0.5f,	0.5f,	-0.5f},
		{-0.5f, -0.5f,	-0.5f},
		{0.5f,	-0.5f,	-0.5f},
		{0.5f,	0.5f,	-0.5f}
	},
	{	// right
		{0.5f,	0.5f,	-0.5f},
		{0.5f,	-0.5f,	-0.5f},
		{0.5f,	-0.5f,	0.5f},
		{0.5f,	0.5f,	0.5f}
	},
	{	// Left
		{-0.5f,	0.5f,	0.5f},
		{-0.5f, -0.5f,	0.5f},
		{-0.5f,	-0.5f,	-0.5f},
		{-0.5f,	0.5f,	-0.5f}
	},
	{	// Top
		{0.5f,	 0.5f,	-0.5f},
		{0.5f,	 0.5f,	0.5f},
		{-0.5f,	 0.5f,	0.5f},
		{-0.5f,  0.5f,	-0.5f}
	},
	{	// Bottom
		{-0.5f,	-0.5f,	-0.5f},
		{-0.5f, -0.5f,	0.5f},
		{0.5f,	-0.5f,	0.5f},
		{0.5f,	-0.5f,	-0.5f}
	},
};

static uint s_indices[] = {
	0, 3, 1,
	1, 3, 2
};

void Chunk::Render(RenderSettings::DrawMode drawMode) const
{
	if (!m_meshGenerated)
		return;

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
					glm::perlin((glm::vec2(chunkPos.x, chunkPos.z) * float(CHUNK_UNIT_SIZE) + glm::vec2(x, z) / float(UNIT_VOXEL_RESOLUTION)) / 13.f) * 8.f)
				{
					m_voxels[x][y][z] = Air;
				}
				else 
				{
					m_voxels[x][y][z] = Dirt;
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
	for (uint x = 0; x < CHUNK_VOXEL_SIZE; x++)
	{
		for (uint y = 0; y < CHUNK_VOXEL_SIZE; y++)
		{
			for (uint z = 0; z < CHUNK_VOXEL_SIZE; z++)
			{
				if (m_voxels[x][y][z] == Dirt)
				{
					glm::vec3 offset{ x,y,z };
					for (uint i = 0; i < BlockFace::NumFaces; i++)
					{
						uint neighborX = x + s_blockNormals[i].x;
						uint neighborY = y + s_blockNormals[i].y;
						uint neighborZ = z + s_blockNormals[i].z;

						if (!(neighborX < 0 || neighborX >= CHUNK_VOXEL_SIZE ||
							neighborY < 0 || neighborY >= CHUNK_VOXEL_SIZE ||
							neighborZ < 0 || neighborZ >= CHUNK_VOXEL_SIZE))
						{
							if (m_voxels[neighborX][neighborY][neighborZ] == Dirt)
								continue;
						}
						else // were on the edge of the chunk
						{
							// compare with neighboring chunk, if it exists

						}

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

	m_meshGenerated = 1;
}
