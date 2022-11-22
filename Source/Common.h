#pragma once

// TODO:: this stuff should really not be here. separate into other files.
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define IMGUI_ENABLED

typedef unsigned int uint;

#define PI 3.14159f

// how many voxels per chunk
constexpr int CHUNK_VOXEL_SIZE = 32;
// how many voxels per unit of space    TODO::this is broken
constexpr int UNIT_VOXEL_RESOLUTION = 2;
// how many units does a chunk take up
constexpr int CHUNK_UNIT_SIZE = CHUNK_VOXEL_SIZE / UNIT_VOXEL_RESOLUTION;
constexpr float VOXEL_UNIT_SIZE = 1.0f / UNIT_VOXEL_RESOLUTION;

static_assert(CHUNK_VOXEL_SIZE % UNIT_VOXEL_RESOLUTION == 0);


struct InputData
{
	void Reset()
	{
		m_moveInput = glm::vec3(0.0f);
		m_mouseInput = glm::vec2(0.0f);
		m_mouseButtons = glm::vec2(0.0f);
	}

	bool m_disableMouseLook = false;

	glm::vec3 m_moveInput = glm::vec3(0.0f);
	glm::vec2 m_mouseInput = glm::vec2(0.0f);
	glm::vec2 m_mouseButtons = glm::vec2(0.0f);
	glm::vec2 m_mouseWheel = glm::vec2(0.0f);
};

struct VertexPCN
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
};

struct VertexP
{
	glm::vec3 position;
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

const static BlockFace s_opposingBlockFaces[BlockFace::NumFaces]
{
	BlockFace::Left,
	BlockFace::Right,
	BlockFace::Bottom,
	BlockFace::Top,
	BlockFace::Back,
	BlockFace::Front
};

const static glm::vec3 s_blockNormals[BlockFace::NumFaces] = {
	{ 1, 0, 0 },	// Right
	{ -1, 0, 0 },	// Left
	{ 0, 1, 0 },	// Top
	{ 0, -1, 0 },	// Bottom
	{ 0, 0, 1 },	// Front 
	{ 0, 0, -1 },	// Back
};

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

static glm::vec3 s_centeredFaces[BlockFace::NumFaces][4] =
{
	{	// right
		{1.0f,	1.0f,	-1.0f},
		{1.0f,	-1.0f,	-1.0f},
		{1.0f,	-1.0f,	1.0f},
		{1.0f,	1.0f,	1.0f}
	},
	{	// Left
		{-1.0f,	1.0f,	1.0f},
		{-1.0f,	-1.0f,	1.0f},
		{-1.0f,	-1.0f,	-1.0f},
		{-1.0f,	1.0f,	-1.0f}
	},
	{	// Top
		{1.0f,	1.0f,	-1.0f},
		{1.0f,	1.0f,	1.0f},
		{-1.0f,	1.0f,	1.0f},
		{-1.0f,	1.0f,	-1.0f}
	},
	{	// Bottom
		{-1.0f,	-1.0f,	-1.0f},
		{-1.0f,	-1.0f,	1.0f},
		{1.0f,	-1.0f,	1.0f},
		{1.0f,	-1.0f,	-1.0f}
	},
	{	// Front
		{1.0f,	1.0f,	1.0f},
		{1.0f,  -1.0f,	1.0f},
		{-1.0f,	-1.0f,	1.0f},
		{-1.0f,	1.0f,	1.0f}
	},
	{	// Back
		{-1.0f,	1.0f,	-1.0f},
		{-1.0f,	-1.0f,	-1.0f},
		{1.0f,	-1.0f,	-1.0f},
		{1.0f,	1.0f,	-1.0f}
	},
};

static uint s_indices[] = {
	0, 3, 1,
	1, 3, 2
};

struct Ray
{
	glm::vec3 origin = glm::vec3(0);
	glm::vec3 dir = glm::vec3(0);
};

struct Plane
{
	Plane() {};
	Plane(const glm::vec3& p1, const glm::vec3& norm)
		: n(glm::normalize(norm)), 
		d(glm::dot(n, p1))
	{}

	// unit vector
	glm::vec3 n = { 0.f, 1.f, 0.f };
	// distance from origin to the nearest point in the plan
	float d = 0.f;

	float getSignedDistanceToPlan(const glm::vec3& p) const
	{
		return glm::dot(n, p) - d;
	}
};

enum WorldFlags
{
	EnableMT = 0x00000001,
};

struct Frustum
{
	Plane topFace;
	Plane bottomFace;
	
	Plane rightFace;
	Plane leftFace;
	
	Plane farFace;
	Plane nearFace;
};

struct BoundingVolume
{
	virtual bool IsInFrustum(const Frustum& camFrustum, const glm::mat4& transform) const = 0;
	virtual bool IsInFrustumTranslateOnly(const Frustum& camFrustum, const glm::vec3& translate) const = 0;
	virtual bool IsInFrustumWorldspace(const Frustum& camFrustum) const = 0;
};

struct AABB : public BoundingVolume
{
	glm::vec3 min = glm::vec3(0);
	glm::vec3 max = glm::vec3(0);

	AABB() {};

	AABB(const glm::vec3& minP, const glm::vec3& maxP)
		: BoundingVolume{}, min(minP), max(maxP)
	{};

	AABB(const glm::vec3& inCenter, float iI, float iJ, float iK)
	{
		glm::vec3 extents = glm::vec3(iI, iJ, iK) / 2.f;
		min = inCenter - extents;
		max = inCenter + extents;
	};

	bool IsInFrustum(const Frustum& camFrustum, const glm::mat4& transform) const override
	{
		//TODO:: need a function that doesnt require a transform other than a translate
		//Get global scale thanks to our transform
		glm::vec3 halfExtents = (max - min) / 2.0f;
		const glm::vec3 globalCenter{ transform * glm::vec4(min + halfExtents, 1.f) };

		// Scaled orientation
		const glm::vec3 right = (glm::mat3(transform) * glm::vec3(1,0,0)) * halfExtents.x;
		const glm::vec3 up = (glm::mat3(transform) * glm::vec3(0,1,0)) * halfExtents.y;
		const glm::vec3 forward = (glm::mat3(transform) * glm::vec3(0,0,1)) * halfExtents.z;

		const float newIi = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));

		const float newIj = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));

		const float newIk = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

		//We not need to divise scale because it's based on the half extention of the AABB
		const AABB globalAABB(globalCenter, newIi, newIj, newIk);

		return (globalAABB.isOnOrForwardPlan(camFrustum.leftFace) &&
			globalAABB.isOnOrForwardPlan(camFrustum.rightFace) &&
			globalAABB.isOnOrForwardPlan(camFrustum.topFace) &&
			globalAABB.isOnOrForwardPlan(camFrustum.bottomFace) &&
			globalAABB.isOnOrForwardPlan(camFrustum.nearFace) &&
			globalAABB.isOnOrForwardPlan(camFrustum.farFace));
	};

	bool IsInFrustumTranslateOnly(const Frustum& camFrustum, const glm::vec3& translate) const override
	{
		const AABB globalAABB(min + translate, max + translate);
		glm::vec3 halfExtents = (max - min) / 2.0f;

		return (globalAABB.isOnOrForwardPlan(camFrustum.leftFace, halfExtents) &&
			globalAABB.isOnOrForwardPlan(camFrustum.rightFace, halfExtents) &&
			globalAABB.isOnOrForwardPlan(camFrustum.topFace, halfExtents) &&
			globalAABB.isOnOrForwardPlan(camFrustum.bottomFace, halfExtents) &&
			globalAABB.isOnOrForwardPlan(camFrustum.nearFace, halfExtents) &&
			globalAABB.isOnOrForwardPlan(camFrustum.farFace, halfExtents));
	}

	bool IsInFrustumWorldspace(const Frustum& camFrustum) const override
	{
		glm::vec3 halfExtents = (max - min) / 2.0f;
		return (isOnOrForwardPlan(camFrustum.leftFace, halfExtents) &&
			isOnOrForwardPlan(camFrustum.rightFace, halfExtents) &&
			isOnOrForwardPlan(camFrustum.topFace, halfExtents) &&
			isOnOrForwardPlan(camFrustum.bottomFace, halfExtents) &&
			isOnOrForwardPlan(camFrustum.nearFace, halfExtents) &&
			isOnOrForwardPlan(camFrustum.farFace, halfExtents));
	}

	bool isOnOrForwardPlan(const Plane& plane) const
	{
		glm::vec3 halfExtents = (max - min) / 2.0f;
		// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
		const float r = glm::dot(halfExtents, glm::abs(plane.n));
		return -r <= plane.getSignedDistanceToPlan(min + halfExtents);
	};

	bool isOnOrForwardPlan(const Plane& plane, const glm::vec3& halfExtents) const
	{
		// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
		const float r = glm::dot(halfExtents, glm::abs(plane.n)); 
		return -r <= plane.getSignedDistanceToPlan(min + halfExtents);
	};

	void Translate(const glm::vec3& translate)
	{
		min += translate;
		max += translate;
	}

	glm::vec3 Extents()
	{
		return max - min;
	}
};
