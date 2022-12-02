#include "VoxelScene.h"
#include <glad/glad.h>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include "Camera.h"
#include <math.h>

#ifdef DEBUG
#define NOMINMAX
#include <windows.h>
#endif
#ifdef IMGUI_ENABLED
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif


ShaderProgram VoxelScene::s_chunkShaderProgram;
ShaderProgram VoxelScene::s_debugWireframeShaderProgram;
VoxelScene::ImguiData VoxelScene::s_imguiData;

#ifdef DEBUG
const uint RENDER_DISTANCE = 10;
#else
const uint RENDER_DISTANCE = 15;
#endif

VoxelScene::VoxelScene()
{
	m_chunks = std::unordered_map<glm::i32vec3, Chunk*>();

	//m_noiseGenerator = FastNoise::New<FastNoise::FractalFBm>();
	//auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
	//m_noiseGenerator->SetSource(fnSimplex);
	//m_noiseGenerator->SetGain(0.1f);
	//m_noiseGenerator->SetLacunarity(10);
	//m_noiseGenerator->SetOctaveCount(3);
	m_noiseGenerators.noiseGenerator = FastNoise::NewFromEncodedNodeTree("EQADAAAAAAAAQBAAAAAAPxkADQADAAAAAAAAQAkAAAAAAD8AAAAAAAEEAAAAAABI4TpAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAPwAAAAAA");
	m_noiseGenerators.noiseGeneratorCave = FastNoise::NewFromEncodedNodeTree("DQACAAAAAAAAQBoAAJqZGb8BGwAPAAIAAAAAAABADQACAAAAAAAAQAkAAAAAAD8AAAAAAAAAAAA/AAAAAAAAAACAvwAAAAA/AAAAAAA=");
	m_noiseGenerators.biomeGenerator = FastNoise::NewFromEncodedNodeTree("CgADAAAAAAAAAAAAAIA/");
	//auto fnSimplex2 = FastNoise::New<FastNoise::Simplex>();
	//m_noiseGeneratorCave->SetSource(fnSimplex2);
	//m_noiseGeneratorCave->SetOctaveCount(2);

	// if we can ever have more than one voxel scene move this.
	Chunk::InitShared(
		m_threadPool.GetThreadIDs(),
		std::bind(&VoxelScene::AddToMeshListCallback, this, std::placeholders::_1),
		std::bind(&VoxelScene::AddToRenderListCallback, this, std::placeholders::_1),
		&m_chunkGenParams
	);

	glGenVertexArrays(1, &m_chunkVAO);
	//during initialization
	glBindVertexArray(m_chunkVAO);

	//https://riptutorial.com/opengl/example/28662/version-4-3
	constexpr int vertexBindingPoint = 0;// free to choose, must be less than the GL_MAX_VERTEX_ATTRIB_BINDINGS limit

	glVertexAttribIFormat(0, 1, GL_UNSIGNED_INT, 0);
	// set the details of a single attribute
	glVertexAttribBinding(0, vertexBindingPoint);
	// which buffer binding point it is attached to
	glEnableVertexAttribArray(0);

	//glVertexAttribFormat(1, 3, GL_FLOAT, false, offsetof(VertexPCN, color));
	//glVertexAttribBinding(1, vertexBindingPoint);
	//glEnableVertexAttribArray(1);

	//glVertexAttribFormat(2, 3, GL_FLOAT, false, offsetof(VertexPCN, normal));
	//glVertexAttribBinding(2, vertexBindingPoint);
	//glEnableVertexAttribArray(2);

	glGenBuffers(1, &m_aabbVBO);
	glGenBuffers(1, &m_aabbEBO);

	glGenVertexArrays(1, &m_debugWireframeVAO);
	glBindVertexArray(m_debugWireframeVAO);
	glVertexAttribFormat(0, 3, GL_FLOAT, false, offsetof(VertexP, position));
	glVertexAttribBinding(0, vertexBindingPoint);
	glEnableVertexAttribArray(0);

	for (uint i = 0; i < BlockFace::NumFaces; i++)
	{
		for (uint j = 0; j < 4; j++)
		{
			m_aabbVerts.push_back(VertexP{ (s_faces[i][j] * float(CHUNK_UNIT_SIZE)) });
		}
		for (uint j = 0; j < 6; j++)
		{
			m_aabbIndices.push_back(s_indices[j] + m_aabbVertexCount);
		}
		m_aabbIndexCount += 6;
		m_aabbVertexCount += 4;
	}

	glBindVertexArray(m_debugWireframeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_aabbVBO);
	glBufferData(GL_ARRAY_BUFFER, m_aabbVertexCount * sizeof(VertexP), (float*)m_aabbVerts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_aabbEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_aabbIndexCount * sizeof(uint), m_aabbIndices.data(), GL_STATIC_DRAW);
}

VoxelScene::~VoxelScene()
{
	m_threadPool.ClearJobPool();
	m_threadPool.WaitForAllThreadsFinished();

	for (auto& chunk : m_chunks)
		delete chunk.second;

	Chunk::DeleteShared();
}

void VoxelScene::InitShared()
{
	s_chunkShaderProgram = ShaderProgram("terrain.vs.glsl", "terrain.fs.glsl");
	s_debugWireframeShaderProgram = ShaderProgram("DebugWireframe.vs.glsl", "DebugWireframe.fs.glsl");
}

Chunk* VoxelScene::CreateChunk(const glm::i32vec3& chunkPos)
{
	if (m_chunks[chunkPos])
		return nullptr;

	// TODO:: use smart pointers
	Chunk* chunk = new Chunk(chunkPos, 0/*, &m_chunkGenParams*/);
	m_chunks[chunkPos] = chunk;
	
	return chunk;
}

inline void VoxelScene::NotifyNeighbor(Chunk* chunk, glm::i32vec3 pos, BlockFace side, BlockFace oppositeSide)
{
	if (Chunk* neighbor = m_chunks[pos + glm::i32vec3(s_blockNormals[side])])
	{
		// this might be too slow to wait for locks
		neighbor->UpdateNeighborRef(oppositeSide, chunk);
		chunk->UpdateNeighborRefNewChunk(side, neighbor);
	}
}

void VoxelScene::Update(const glm::vec3& position)
{
	ZoneScoped;
	if (m_chunkGenParams != m_chunkGenParamsNext)
	{
		m_chunkGenParams = m_chunkGenParamsNext;
		ResetVoxelScene();
		//m_noiseGenerator->SetLacunarity(m_chunkGenParamsNext.terrainLacunarity);
		//m_noiseGenerator->SetGain(m_chunkGenParamsNext.terrainGain);
		//m_noiseGenerator->SetOctaveCount(m_chunkGenParamsNext.terrainOctaves);
	}
	m_frameChunks.clear();
	std::vector<Chunk*> newChunks;
	m_octree.GenerateFromPosition(position, newChunks, m_frameChunks);
	for (Chunk* chunk : newChunks)
	{
		m_threadPool.Submit(std::bind(&Chunk::GenerateVolume, chunk, &m_noiseGenerators), chunk->GetLOD() == 0 ? Priority_High : Priority_Med);
	}

	if (!RenderSettings::Get().mtEnabled)
	{
		Job j;
		uint count = 0;
		while (m_threadPool.GetPoolJob(j))
		{
			j.func();
			if (count++ > 100)
				break;
		}
	}
}

void VoxelScene::ResetVoxelScene()
{
	m_threadPool.ClearJobPool();
	m_threadPool.WaitForAllThreadsFinished();
	for (auto& chunk : m_chunks)
		delete chunk.second;
	m_chunks.clear();

	m_generateMeshList.clear();
	m_generateMeshCallbackList.clear();
	m_renderList.clear();
	m_renderCallbackList.clear();

	m_currentGenerateRadius = 3;
	m_lastGenerateRadius = 0;
	m_lastGeneratePos = glm::vec3(0, 0, 0);
	m_lastGeneratedChunkPos = glm::i32vec3(UINT_MAX, UINT_MAX, UINT_MAX);

	m_octree.Clear();
}

void VoxelScene::Render(const Camera* camera, const Camera* debugCullCamera)
{
	ZoneNamed(SetupRender, true);
	s_chunkShaderProgram.Use();
	glUniformMatrix4fv(2, 1, GL_FALSE, &camera->GetProjMatrix()[0][0]);
	glUniformMatrix4fv(1, 1, GL_FALSE, &camera->GetViewMatrix()[0][0]);
	glUniform3fv(50, 1, &camera->GetPosition()[0]);

	RenderSettings::DrawMode drawMode = RenderSettings::Get().m_drawMode;

	if (!m_useOctree)
	{
		m_renderCallbackListMutex.lock();
		if (m_renderCallbackList.size())
			m_renderList.insert(m_renderList.end(), m_renderCallbackList.begin(), m_renderCallbackList.end());
		m_renderCallbackList.clear();
		m_renderCallbackListMutex.unlock();

		// throw this on a thread? should only sort culled chunks or just not sort at all.
		glm::vec3 chunkPos = m_lastGeneratedChunkPos;
		m_renderList.sort([chunkPos](const Chunk* a, const Chunk* b)
			{ return glm::length2(chunkPos - a->m_chunkPos) < glm::length2(chunkPos - b->m_chunkPos); }
		);
	}

	
	uint vertexCount = 0;
	uint numRenderChunks = 0;
	double totalGenTime = 0;

	ZoneNamed(Render, true);

	glm::mat4 modelMat;
	glBindVertexArray(m_chunkVAO);
	glDepthMask(GL_TRUE);
	
	for (Chunk* chunk : m_frameChunks)
	{
		if (chunk == nullptr || !chunk->Renderable())
			continue;

		if (chunk->IsInFrustum(debugCullCamera->GetFrustum()))
		{
			vertexCount += chunk->GetVertexCount();
			totalGenTime += chunk->m_genTime;
			numRenderChunks++;
			chunk->Render(drawMode);
		}
	}
	s_imguiData.numTotalChunks = m_frameChunks.size();
	s_imguiData.numRenderChunks = numRenderChunks;
	s_imguiData.numVerts = vertexCount;
	s_imguiData.avgChunkGenTime = totalGenTime / s_imguiData.numRenderChunks;
}

void VoxelScene::RenderTransparency(const Camera* camera, const Camera* debugCullCamera)
{
	if (RenderSettings::Get().renderDebugWireframes)
	{
		RenderDebugBoundingBoxes(camera, debugCullCamera);
	}
	RenderHitPos(camera, debugCullCamera);
}

static int TestAABBAABB(AABB a, AABB b)
{
	// Exit with no intersection if separated along an axis
	if (a.max[0] < b.min[0] || a.min[0] > b.max[0]) return 0;
	if (a.max[1] < b.min[1] || a.min[1] > b.max[1]) return 0;
	if (a.max[2] < b.min[2] || a.min[2] > b.max[2]) return 0;
	// Overlapping on all axes means AABBs are intersecting
	return 1; 
}

// Intersect AABBs ‘a’ and ‘b’ moving with constant velocities va and vb.
// On intersection, return time of first and last contact in tfirst and tlast
static int IntersectMovingAABBAABB(
	const AABB& a, 
	const AABB& b, 
	const glm::vec3& va, 
	const glm::vec3& vb, 
	uint8_t slideMaskB,
	float& tfirst,
	float& tlast,
	int& intersectionAxis
)
{
	// Exit early if ‘a’ and ‘b’ initially overlapping
	if (TestAABBAABB(a, b)) {
		tfirst = tlast = 0.0f;
		fprintf(stderr, "overlapping\n");
		return 1;
	}

	// Use relative velocity; effectively treating ’a’ as stationary
	glm::vec3 v = vb - va;

	// Initialize times of first and last contact
	tfirst = 0.0f;
	tlast = 1.0f;

	// For each axis, determine times of first and last contact, if any
	for (int i = 0; i < 3; i++) {
		if (v[i] < 0.0f) {
			if (b.max[i] < a.min[i]) return 0; // Nonintersecting and moving apart
			if (a.max[i] < b.min[i]) tfirst = glm::max((a.max[i] - b.min[i]) / v[i], tfirst);
			if (b.max[i] > a.min[i]) tlast = glm::min((a.min[i] - b.max[i]) / v[i], tlast);
		}
		if (v[i] > 0.0f) {
			if (b.min[i] > a.max[i]) return 0; // Nonintersecting and moving apart
			if (b.max[i] < a.min[i]) tfirst = glm::max((a.min[i] - b.max[i]) / v[i], tfirst);
			if (a.max[i] > b.min[i]) tlast = glm::min((a.max[i] - b.min[i]) / v[i], tlast);
		}
		// No overlap possible if time of first contact occurs after time of last contact
		if (tfirst > tlast) return 0;
	}
	return 1;
}

// Intersect AABBs ‘a’ and ‘b’ moving with constant velocities va and vb.
// On intersection, return time of first and last contact in tfirst and tlast
static int IntersectMovingAABBAABBAxis(
	const AABB& a,
	const AABB& b,
	const glm::vec3& va,
	const glm::vec3& vb,
	uint8_t slideMaskB,
	float& tfirst,
	float& tlast,
	int& i
)
{
	// Exit early if ‘a’ and ‘b’ initially overlapping
	if (TestAABBAABB(a, b)) {
		tfirst = tlast = 0.0f;
		fprintf(stderr, "overlapping\n");
		return 1;
	}

	// Use relative velocity; effectively treating ’a’ as stationary
	glm::vec3 v = vb - va;

	// Initialize times of first and last contact
	tfirst = 0.0f;
	tlast = 1.0f;
	int i2 = (i + 1) % 3;
	int i3 = (i + 2) % 3;

	// this isnt fully correct. this is just checking initial positions. need to also cast this movement?
	if (a.max[i2] < b.min[i2] || a.min[i2] > b.max[i2]) return 0;
	if (a.max[i3] < b.min[i3] || a.min[i3] > b.max[i3]) return 0;

	// For each axis, determine times of first and last contact, if any
	if (v[i] < 0.0f) {
		if (b.max[i] < a.min[i]) return 0; // Nonintersecting and moving apart
		if (a.max[i] < b.min[i]) tfirst = glm::max((a.max[i] - b.min[i]) / v[i], tfirst);
		if (b.max[i] > a.min[i]) tlast = glm::min((a.min[i] - b.max[i]) / v[i], tlast);
	}
	if (v[i] > 0.0f) {
		if (b.min[i] > a.max[i]) return 0; // Nonintersecting and moving apart
		if (b.max[i] < a.min[i]) tfirst = glm::max((a.min[i] - b.max[i]) / v[i], tfirst);
		if (a.max[i] > b.min[i]) tlast = glm::min((a.max[i] - b.min[i]) / v[i], tlast);
	}
	// No overlap possible if time of first contact occurs after time of last contact
	if (tfirst > tlast) return 0;
	return 1;
}

void VoxelScene::ResolveBoxCollider(BoxCollider& collider, float timeDelta)
{
	const AABB& aabb = collider.GetAABB();
	//const glm::vec3 extents = aabb.max - aabb.min;

	glm::vec3 velocity = collider.GetVelocity();
	if (!collider.GetSlideMask(BlockFace::Bottom))
	{
		velocity = velocity + glm::vec3(0, -100, 0) * (timeDelta / 1000.f);
		collider.SetVelocity(velocity);
	}
	glm::vec3 targetDir = velocity * (timeDelta / 1000.f);
	for (int i = 0; i < 3; i++)
	{
		if (targetDir[i] == 0.0f)
		{
			continue;
		}
		int index = i * 2 + (targetDir[i] > 0.0f ? 0 : 1);
		int otherIndex = i * 2 + (targetDir[i] > 0.0f ? 1 : 0);
		if (collider.GetSlideMask(index))
		{
			targetDir[i] = 0;
		}
		collider.ClearSlideMask(otherIndex);
	}

	AABB translatedAABB = aabb;
	translatedAABB.Translate(targetDir);

	glm::i32vec3 voxelSpaceNewMin = translatedAABB.min * float(UNIT_VOXEL_RESOLUTION);
	glm::i32vec3 voxelSpaceStartMin = aabb.min * float(UNIT_VOXEL_RESOLUTION);
	glm::i32vec3 voxelSpaceNewMax = translatedAABB.min * float(UNIT_VOXEL_RESOLUTION);
	glm::i32vec3 voxelSpaceStartMax = aabb.min * float(UNIT_VOXEL_RESOLUTION);

	// need to also check max
	if (voxelSpaceNewMin == voxelSpaceStartMin )
	{
		collider.Translate(targetDir);
		return;
	}

	AABB combinedAABB = AABB(glm::min(aabb.min, translatedAABB.min), glm::max(aabb.max, translatedAABB.max));
	glm::i32vec3 voxelSpaceExtents = glm::ceil(combinedAABB.Extents() * float(UNIT_VOXEL_RESOLUTION) + glm::vec3(1));

	bool* localBlocks = new bool[voxelSpaceExtents.x * voxelSpaceExtents.y * voxelSpaceExtents.z];

	// TODO:: lots of optimizations possible here. not re-grabbing chunks every iteration. better prioritization of AABB checking in second loop
	// should check AABB's closest in direction of movement first. combining neighboring AABBs to do less checks. 
	// can think of combining AABBs as max of 3 planes around AABB. start will always be in a corner and will always be empty. so combine those ones in planes around start on each axis
	glm::vec3 offset = { 0,0,0 };
	glm::vec3 currentPos;
	int index = 0;
	for (int i = 0; i < voxelSpaceExtents.x; i++)
	{
		offset.x = i;
		for (int j = 0; j < voxelSpaceExtents.y; j++)
		{
			offset.y = j;
			for (int k = 0; k < voxelSpaceExtents.z; k++)
			{
				offset.z = k;
				currentPos = combinedAABB.min + offset * VOXEL_UNIT_SIZE;
				// need to solve edge case if outside the octree
				Chunk* currChunk = m_octree.GetChunkAtWorldPos(currentPos);
				if (currChunk == nullptr || !currChunk->IsDeletable() || currChunk->GetLOD() != 0)
					return;

				localBlocks[index++] = currChunk->VoxelIsCollideableAtWorldPos(currentPos);
			}
		}
	}

	index = 0;
	AABB voxelAABB;
	glm::vec3 roundedMin = glm::vec3(glm::i32vec3(combinedAABB.min * float(UNIT_VOXEL_RESOLUTION))) * VOXEL_UNIT_SIZE;
	glm::vec3 voxelAABBMin;
	float tFirst = 0, tLast = 0;
	bool collided = false;
	int intersectionAxis;
	for (int i = 0; i < voxelSpaceExtents.x; i++)
	{
		if (collided) break;
		offset.x = i;
		for (int j = 0; j < voxelSpaceExtents.y; j++)
		{
			if (collided) break;
			offset.y = j;
			for (int k = 0; k < voxelSpaceExtents.z; k++)
			{
				offset.z = k;
				if (localBlocks[index++])
				{
					voxelAABBMin = roundedMin + offset * VOXEL_UNIT_SIZE;
					voxelAABB = { voxelAABBMin, voxelAABBMin + glm::vec3(VOXEL_UNIT_SIZE) };
					// this is potentially problematic. if theres two AABBs in our path, and were going fast enough to collide with both of them. then only one will get collided with. and it could be the further one.
					// need to sort by distance or something. combining would help a bit.
					if (IntersectMovingAABBAABB(voxelAABB, aabb, glm::vec3(0), targetDir, 0, tFirst, tLast, intersectionAxis) != 0)
					{
						collided = true;
						break;
					}
				}
			}
		}
	}

	if (collided)
	{
		collider.Translate(targetDir * glm::min(tFirst, 1.0f) * 0.999f);
		collider.SetVelocityIndex(intersectionAxis, 0);
		collider.SetSlideMask(intersectionAxis);
	}
	else
	{
		collider.Translate(targetDir);
	}

	delete [] localBlocks;
}

void VoxelScene::ResolveBoxCollider2(BoxCollider& collider, float timeDelta)
{
	const AABB& aabb = collider.GetAABB();
	//const glm::vec3 extents = aabb.max - aabb.min;

	glm::vec3 velocity = collider.GetVelocity();
	//if (!collider.GetSlideMask(BlockFace::Bottom))
	{
		velocity = velocity + glm::vec3(0, -75, 0) * (timeDelta / 1000.f);
		collider.SetVelocity(velocity);
	}
	glm::vec3 targetDir = velocity * (timeDelta / 1000.f);

	AABB translatedAABB = aabb;
	translatedAABB.Translate(targetDir);

	glm::i32vec3 voxelSpaceNewMin = translatedAABB.min * float(UNIT_VOXEL_RESOLUTION);
	glm::i32vec3 voxelSpaceStartMin = aabb.min * float(UNIT_VOXEL_RESOLUTION);
	glm::i32vec3 voxelSpaceNewMax = translatedAABB.min * float(UNIT_VOXEL_RESOLUTION);
	glm::i32vec3 voxelSpaceStartMax = aabb.min * float(UNIT_VOXEL_RESOLUTION);

	// need to also check max
	if (voxelSpaceNewMin == voxelSpaceStartMin)
	{
		collider.Translate(targetDir);
		return;
	}

	AABB combinedAABB = AABB(glm::min(aabb.min, translatedAABB.min), glm::max(aabb.max, translatedAABB.max));
	glm::i32vec3 voxelSpaceExtents = glm::ceil(combinedAABB.Extents() * float(UNIT_VOXEL_RESOLUTION) + glm::vec3(1));

	bool* localBlocks = new bool[voxelSpaceExtents.x * voxelSpaceExtents.y * voxelSpaceExtents.z];

	// TODO:: lots of optimizations possible here. not re-grabbing chunks every iteration. better prioritization of AABB checking in second loop
	// should check AABB's closest in direction of movement first. combining neighboring AABBs to do less checks. 
	// can think of combining AABBs as max of 3 planes around AABB. start will always be in a corner and will always be empty. so combine those ones in planes around start on each axis
	glm::vec3 offset = { 0,0,0 };
	glm::vec3 currentPos;
	int index = 0;
	for (int i = 0; i < voxelSpaceExtents.x; i++)
	{
		offset.x = i;
		for (int j = 0; j < voxelSpaceExtents.y; j++)
		{
			offset.y = j;
			for (int k = 0; k < voxelSpaceExtents.z; k++)
			{
				offset.z = k;
				currentPos = combinedAABB.min + offset * VOXEL_UNIT_SIZE;
				// need to solve edge case if outside the octree
				Chunk* currChunk = m_octree.GetChunkAtWorldPos(currentPos);
				if (currChunk == nullptr || !currChunk->IsDeletable() || currChunk->GetLOD() != 0)
					return;

				localBlocks[index++] = currChunk->VoxelIsCollideableAtWorldPos(currentPos);
			}
		}
	}

	AABB voxelAABB;
	glm::vec3 roundedMin = glm::vec3(glm::i32vec3(combinedAABB.min * float(UNIT_VOXEL_RESOLUTION))) * VOXEL_UNIT_SIZE;
	glm::vec3 voxelAABBMin;
	float tFirst[3] = { 1, 1, 1 }, tLast[3] = { 1, 1, 1 };
	float tFirstTemp = 0, tLastTemp = 0;
	bool collided[3] = { false };
	int intersectionAxis;
	for (int a = 0; a < 3; a++)
	{
		index = 0;
		if (targetDir[a] == 0.0f)
			continue;
		glm::vec3 targetDirElement = glm::vec3(0);
		targetDirElement[a] = targetDir[a];
		for (int i = 0; i < voxelSpaceExtents.x; i++)
		{
			offset.x = i;
			for (int j = 0; j < voxelSpaceExtents.y; j++)
			{
				offset.y = j;
				for (int k = 0; k < voxelSpaceExtents.z; k++)
				{
					offset.z = k;
					if (localBlocks[index++])
					{
						voxelAABBMin = roundedMin + offset * VOXEL_UNIT_SIZE;
						voxelAABB = { voxelAABBMin, voxelAABBMin + glm::vec3(VOXEL_UNIT_SIZE) };
						// this is potentially problematic. if theres two AABBs in our path, and were going fast enough to collide with both of them. then only one will get collided with. and it could be the further one.
						// need to sort by distance or something. combining would help a bit.
						if (IntersectMovingAABBAABBAxis(voxelAABB, aabb, glm::vec3(0), targetDirElement, 0, tFirstTemp, tLastTemp, a) != 0)
						{
							collided[a] = true;
							tFirst[a] = glm::min(tFirst[a], tFirstTemp);
							tLast[a] = glm::min(tLast[a], tLastTemp);
						}
					}
				}
			}
		}
		if (collided[a])
		{
			//collider.Translate(targetDirElement * glm::min(tFirst[a], 1.0f) * 0.999f);
			collider.SetVelocityIndex(a, 0);
		}
		else
		{
			collider.Translate(targetDirElement);
		}
	}

	//for (int a = 0; a < 3; a++)
	//{
	//	glm::vec3 targetDirElement = glm::vec3(0);
	//	targetDirElement[a] = targetDir[a];
	//	if (collided[a])
	//	{
	//		//collider.Translate(targetDirElement * glm::min(tFirst[a], 1.0f) * 0.999f);
	//		collider.SetVelocityIndex(a, 0);
	//	}
	//	else
	//	{
	//		collider.Translate(targetDirElement);
	//	}
	//}

	fprintf(stderr, "Colliding on %d, %d, %d\n", collided[0], collided[1], collided[2]);

	delete[] localBlocks;
}

bool VoxelScene::RayCast(const Ray& ray, VoxelRayHit& voxelRayHit)
{
	// need to solve edge case if outside the octree
	Chunk* currChunk = m_octree.GetChunkAtWorldPos(ray.origin);
	if (currChunk == nullptr || !currChunk->IsDeletable() || currChunk->GetLOD() != 0)
		return false;

	glm::i32vec3 voxelIndex;
	bool ret = currChunk->GetVoxelIndexAtWorldPos(ray.origin, voxelIndex);
	const int stepX = ray.dir.x > 0 ? 1 : (ray.dir.x < 0 ? -1 : 0);
	const int stepY = ray.dir.y > 0 ? 1 : (ray.dir.y < 0 ? -1 : 0);
	const int stepZ = ray.dir.z > 0 ? 1 : (ray.dir.z < 0 ? -1 : 0);
	const glm::i32vec3 step(stepX, stepY, stepZ);

	const float tDeltaX = VOXEL_UNIT_SIZE / std::max(std::abs(ray.dir.x), 0.000001f);
	const float tDeltaY = VOXEL_UNIT_SIZE / std::max(std::abs(ray.dir.y), 0.000001f);
	const float tDeltaZ = VOXEL_UNIT_SIZE / std::max(std::abs(ray.dir.z), 0.000001f);

	glm::vec3 voxelBorder = glm::vec3(voxelIndex + glm::max(step, 0)) * VOXEL_UNIT_SIZE;
	glm::vec3 tMax = (voxelBorder - (ray.origin - currChunk->GetChunkPos()));
	tMax.x = ray.dir.x != 0.0f ? tMax.x / std::abs(ray.dir.x) : 1000000.f;
	tMax.y = ray.dir.y != 0.0f ? tMax.y / std::abs(ray.dir.y) : 1000000.f;
	tMax.z = ray.dir.z != 0.0f ? tMax.z / std::abs(ray.dir.z) : 1000000.f;

	bool exitedChunk = false;
	float lastT = 0;
	bool collided = false;
	while (!(collided = currChunk->VoxelIsCollideable(voxelIndex)) && lastT < 100.0f)
	{
		if (tMax.x < tMax.y)
		{
			if (tMax.x < tMax.z)
			{
				voxelIndex.x += stepX;
				exitedChunk = (voxelIndex.x >= CHUNK_VOXEL_SIZE || voxelIndex.x < 0);
				lastT = tMax.x;
				tMax.x += tDeltaX;
			}
			else
			{
				voxelIndex.z += stepZ;
				exitedChunk = (voxelIndex.z >= CHUNK_VOXEL_SIZE || voxelIndex.z < 0);
				lastT = tMax.z;
				tMax.z += tDeltaZ;

			}
		}
		else 
		{
			if (tMax.y < tMax.z)
			{
				voxelIndex.y += stepY;
				exitedChunk = (voxelIndex.y >= CHUNK_VOXEL_SIZE || voxelIndex.y < 0);
				lastT = tMax.y;
				tMax.y += tDeltaY;
			}
			else 
			{
				voxelIndex.z += stepZ;
				exitedChunk = (voxelIndex.z >= CHUNK_VOXEL_SIZE || voxelIndex.z < 0);
				lastT = tMax.z;
				tMax.z += tDeltaZ;
			}
		}
		if (exitedChunk)
		{
			// bump a little in that direction to make sure were in that new chunk and not right on the edge
			glm::vec3 currPos = ray.origin + ray.dir * (lastT + 0.001f);
			currChunk = m_octree.GetChunkAtWorldPos(currPos);
			if (currChunk == nullptr || !currChunk->IsDeletable() || currChunk->GetLOD() != 0)
				return false;
			currChunk->GetVoxelIndexAtWorldPos(currPos, voxelIndex);
			exitedChunk = false;
		}
	}
	hitPos = currChunk->GetChunkPos() + (glm::vec3(voxelIndex)/* + glm::vec3(0.5f)*/) * VOXEL_UNIT_SIZE;
	//if (collided)
	//	fprintf(stderr, "hit block type %d\n", uint8_t(currChunk->GetBlockType(voxelIndex.x, voxelIndex.y, voxelIndex.z)));
	if (collided)
	{
		voxelRayHit = 
		{
			voxelIndex,
			currChunk,
			ray.origin + ray.dir * lastT,
			glm::vec3(0),
			currChunk->GetChunkPos() + (glm::vec3(voxelIndex)/* + glm::vec3(0.5f)*/) * VOXEL_UNIT_SIZE * currChunk->GetScale()
		};
	}

	return collided;
}

bool VoxelScene::DeleteBlock(const Ray& ray)
{
	VoxelRayHit hit;
	if (RayCast(ray, hit))
	{
		hit.chunk->DeleteBlockAtIndex(hit.voxelIndex);
		hit.chunk->GenerateMesh();

		for (int i = 0; i < 3; i++)
		{
			glm::vec3 dir = glm::vec3(0);
			dir[i] = 1;

			if (hit.voxelIndex[i] >= CHUNK_VOXEL_SIZE - 1)
			{
				Chunk* neighborChunk = m_octree.GetChunkAtWorldPos(hit.voxelHitPosition + dir * float(1u << hit.chunk->GetLOD()) * 1.5f);
				glm::i8vec3 neighborIndex = hit.voxelIndex;
				neighborIndex += glm::i8vec3(1);
				neighborIndex[i] = 0;
				neighborChunk->DeleteBlockAtInternalIndex(neighborIndex);
				neighborChunk->GenerateMesh();
			}
			else if (hit.voxelIndex[i] <= 0)
			{
				Chunk* neighborChunk = m_octree.GetChunkAtWorldPos(hit.voxelHitPosition - dir * float(1u << hit.chunk->GetLOD()) * .5f);
				glm::i8vec3 neighborIndex = hit.voxelIndex;
				neighborIndex += glm::i8vec3(1);
				neighborIndex[i] = Chunk::INT_CHUNK_VOXEL_SIZE - 1;
				neighborChunk->DeleteBlockAtInternalIndex(neighborIndex);
				neighborChunk->GenerateMesh();
			}
		}
	}
	return true;
}

// TODO:: turn this into an indexed draw on single cube mesh
void VoxelScene::RenderDebugBoundingBoxes(const Camera* camera, const Camera* debugCullCamera)
{
	s_debugWireframeShaderProgram.Use();
	glDepthMask(GL_FALSE);
	glBindVertexArray(m_debugWireframeVAO);
	glBindVertexBuffer(0, m_aabbVBO, 0, sizeof(VertexP));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_aabbEBO);

	//glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * glm::mat4(1)));
	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(camera->GetProjMatrix()));

	glm::mat4x4 modelMat;
	for (Chunk* chunk : m_frameChunks)
	{
		if (chunk->IsEmpty())
			continue;

		chunk->GetModelMat(modelMat);
		glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * glm::scale(modelMat, glm::vec3(UNIT_VOXEL_RESOLUTION))));

		glDrawElements(GL_LINES, m_aabbIndexCount, GL_UNSIGNED_INT, 0);
	}

	glDepthMask(GL_TRUE);
}

void VoxelScene::RenderHitPos(const Camera* camera, const Camera* debugCullCamera)
{
	s_debugWireframeShaderProgram.Use();
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_debugWireframeVAO);
	glBindVertexBuffer(0, m_aabbVBO, 0, sizeof(VertexP));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_aabbEBO);

	glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(camera->GetProjMatrix()));

	glm::mat4x4 modelMat = glm::mat4(1.0f);
	modelMat = glm::translate(modelMat, hitPos);
	modelMat = glm::scale(modelMat, glm::vec3(VOXEL_UNIT_SIZE));
	modelMat = glm::translate(modelMat, glm::vec3(0.5f));
	modelMat = glm::scale(modelMat, glm::vec3(1.001f));
	modelMat = glm::translate(modelMat, glm::vec3(-0.5f));
	modelMat = glm::scale(modelMat, glm::vec3(float(1.0f / CHUNK_UNIT_SIZE)));

	glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix() * modelMat));

	glDrawElements(GL_TRIANGLES, m_aabbIndexCount, GL_UNSIGNED_INT, 0);

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

#ifdef IMGUI_ENABLED
void VoxelScene::RenderImGui()
{
	ImGui::Text("%d vertices", s_imguiData.numVerts);
	ImGui::Text("%d render chunks", s_imguiData.numRenderChunks);
	ImGui::Text("%d total chunks", s_imguiData.numTotalChunks);
	ImGui::Text("%f avg gen time", s_imguiData.avgChunkGenTime);

	ImGui::SliderFloat("cave frequency", &m_chunkGenParamsNext.caveFrequency, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Terrain Height", &m_chunkGenParamsNext.terrainHeight, 1.f, 2000.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Terrain Lacunarity", &m_chunkGenParamsNext.terrainLacunarity, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Terrain Gain", &m_chunkGenParamsNext.terrainGain, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Terrain Frequency", &m_chunkGenParamsNext.terrainFrequency, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::InputInt("Terrain Octaves", &m_chunkGenParamsNext.terrainOctaves, 1, 10);
	ImGui::Checkbox("Debug Terrain", &m_chunkGenParamsNext.m_debugFlatWorld);
}
#endif

glm::i32vec3 VoxelScene::ConvertWorldPosToChunkPos(const glm::vec3& worldPos)
{
	return worldPos / float(CHUNK_UNIT_SIZE);
}

void VoxelScene::AddToMeshListCallback(Chunk* chunk)
{
	std::lock_guard lock(m_generateMeshCallbackListMutex);
	m_generateMeshCallbackList.push_back(chunk);
}

void VoxelScene::AddToRenderListCallback(Chunk* chunk)
{
	std::lock_guard lock(m_renderCallbackListMutex);
	m_renderCallbackList.push_back(chunk);
}

#ifdef DEBUG
void VoxelScene::ValidateChunks()
{
	int i = RENDER_DISTANCE - 1;
	for (int j = -i; j <= i; j++)
	{
		for (int k = -i; k <= i; k++)
		{
			for (int l = -i; l <= i; l++)
			{
				glm::i32vec3 chunkPos = glm::i32vec3(
					j + m_lastGeneratedChunkPos.x,
					k + m_lastGeneratedChunkPos.y,
					l + m_lastGeneratedChunkPos.z);
				if (Chunk* chunk = m_chunks[chunkPos])
				{
					chunk->m_mutex.lock();
					if (chunk->GetChunkState() < Chunk::ChunkState::GeneratingBuffers)
					{
						char str[128];
						sprintf(str, "Chunk x:%i y:%i z:%i state:%u\n", chunkPos.x, chunkPos.y, chunkPos.z, chunk->GetChunkState());
						OutputDebugString(str);
					}
					chunk->m_mutex.unlock();
				}
			}
		}
	}
}
#endif