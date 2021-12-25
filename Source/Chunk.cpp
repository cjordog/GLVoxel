#include "Chunk.h"
#include <glad/glad.h>
#include <iostream>
#include "glm/gtc/noise.hpp"

Chunk::Chunk(const glm::vec3& chunkPos)
{
	Generate(chunkPos);

	// can bulk generate these from our voxelscene when we create a bunch of chunks.
	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_EBO);

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

static glm::vec3 s_faces[Chunk::NumFaces][4] =
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

static glm::vec3 s_normals[Chunk::NumFaces] = {
	{ 0, 0, 1 },	// Front 
	{ 0, 0, -1 },	// Back
	{ 1, 0, 0 },	// Right
	{ -1, 0, 0 },	// Left
	{ 0, 1, 0 },	// Top
	{ 0, -1, 0 }	// Bottom
};

static uint s_indices[] = {
	0, 3, 1,
	1, 3, 2
};

void Chunk::Render(RenderSettings::DrawMode drawMode)
{
	glBindVertexArray(m_VAO);
	//glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);

	uint dm = (drawMode == RenderSettings::DrawMode::Triangles ? GL_TRIANGLES : GL_LINES);
	glDrawElements(dm, m_indexCount, GL_UNSIGNED_INT, 0);
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
					m_voxels[x][y][z] = Air;
				else
					m_voxels[x][y][z] = Dirt;
			}
		}
	}

	GenerateMesh();
}

void Chunk::GenerateMesh()
{
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
						uint neighborX = x + s_normals[i].x;
						uint neighborY = y + s_normals[i].y;
						uint neighborZ = z + s_normals[i].z;

						if (!(neighborX < 0 || neighborX >= CHUNK_VOXEL_SIZE ||
							neighborY < 0 || neighborY >= CHUNK_VOXEL_SIZE ||
							neighborZ < 0 || neighborZ >= CHUNK_VOXEL_SIZE))
						{
							if (m_voxels[neighborX][neighborY][neighborZ] == Dirt)
								continue;
						}

						for (uint j = 0; j < 4; j++)
						{
							m_vertices.push_back(Vertex{ s_faces[i][j] + offset, glm::vec3(1.0f, 0.0f, 0.0f), s_normals[i] });
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
