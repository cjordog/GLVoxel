#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

typedef unsigned int uint;

#define PI 3.14159f

// how many voxels per chunk
constexpr uint CHUNK_VOXEL_SIZE = 8;
// how many voxels per unit of space
constexpr uint UNIT_VOXEL_RESOLUTION = 1;
// how many units does a chunk take up
constexpr uint CHUNK_UNIT_SIZE = CHUNK_VOXEL_SIZE / UNIT_VOXEL_RESOLUTION;

static_assert(CHUNK_VOXEL_SIZE % UNIT_VOXEL_RESOLUTION == 0);


struct InputData
{
	void Reset()
	{
		m_moveInput = glm::vec3(0.0f);
		m_mouseInput = glm::vec2(0.0f);
		m_mouseButtons = glm::vec2(0.0f);
	}

	glm::vec3 m_moveInput = glm::vec3(0.0f);
	glm::vec2 m_mouseInput = glm::vec2(0.0f);
	glm::vec2 m_mouseButtons = glm::vec2(0.0f);
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
};

enum BlockFace : uint8_t
{
	Right = 0,	// +x
	Left,		// -x
	Top,		// +y
	Bottom,		// -y
	Front,		// +z
	Back,		// -z
	NumFaces
};

const static glm::vec3 s_blockNormals[BlockFace::NumFaces] = {
	{ 1, 0, 0 },	// Right
	{ -1, 0, 0 },	// Left
	{ 0, 1, 0 },	// Top
	{ 0, -1, 0 },	// Bottom
	{ 0, 0, 1 },	// Front 
	{ 0, 0, -1 },	// Back
};