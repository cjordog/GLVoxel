#pragma once

#include "stdint.h"
#include <vector>

#include "Common.h"
#include "RenderSettings.h"

class Chunk
{
public:
	Chunk();

	enum BlockType : uint8_t
	{
		Air = 0,
		Dirt,
		Grass,
		Stone,
	};

	enum BlockFace : uint8_t
	{
		Front = 0,	// +z
		Back,		// -z
		Right,		// +x
		Left,		// -x
		Top,		// +y
		Bottom,		// -y
		NumFaces
	};

	float* GetVertexData() { return (float*)(m_vertices.data()); }
	uint* GetIndexData() { return m_indices.data(); }
	uint GetVertexCount() { return m_vertexCount; }
	uint GetIndexCount() { return m_indexCount; }

	void Render(RenderSettings::DrawMode drawMode);

private:
	void Generate();
	void GenerateMesh();

	BlockType m_voxels[CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE][CHUNK_VOXEL_SIZE] = { BlockType(0) };
	std::vector<Vertex> m_vertices = std::vector<Vertex>();
	std::vector<uint> m_indices = std::vector<uint>();

	uint m_vertexCount = 0;
	uint m_indexCount = 0;

	uint m_VBO = 0;
	uint m_EBO = 0;
	uint m_VAO = 0;
};