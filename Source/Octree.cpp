#include "Octree.h"
#include <stack>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

static glm::vec3 s_centerOctreeOffsets[8] =
{
	{0.5, 0.5, 0.5},
	{0.5, 0.5, -0.5},
	{0.5, -0.5, 0.5},
	{0.5, -0.5, -0.5},
	{-0.5, 0.5, 0.5},
	{-0.5, 0.5, -0.5},
	{-0.5, -0.5, 0.5},
	{-0.5, -0.5, -0.5}
};
static glm::vec3 s_cornerOctreeOffsets[8] =
{
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, -1.0f},
	{0.0f, -1.0f, 0.0f},
	{0.0f, -1.0f, -1.0f},
	{-1.0f, 0.0f, 0.0f},
	{-1.0f, 0.0f, -1.0f},
	{-1.0f, -1.0f, 0.0f},
	{-1.0f, -1.0f, -1.0f}
};

const float CHUNK_LOD_RADIUS = 4;
Octree::Octree()
{
	m_centerPos = glm::vec3(0);
	m_size = 1024;
#ifdef DEBUG
	const int numLODs = 6;
#else
	const int numLODs = 8;
#endif
	m_maxDepth = std::max(numLODs, 1); //log2(size) - 1
	m_root = std::make_shared<OctreeNode>(nullptr, m_centerPos, m_maxDepth);
}

static int GetMaxVectorIndex(const glm::vec3& v)
{
	return (v.x > v.y && v.x > v.z) ? 0 : (v.y > v.z ? 1 : 2);
}

void Octree::GenerateFromPosition(glm::vec3 position, std::vector<Chunk*>& newChunks, std::vector<Chunk*>& leafChunks)
{
	ZoneScoped;
	//chunksToGenerate.reserve(128);
	std::stack<std::shared_ptr<OctreeNode>> nodeStack;
	int currSizeChunks = m_size;
	std::shared_ptr<OctreeNode> currNode;
	glm::vec3 posToChunkCenter;
	nodeStack.push(m_root);
	while (!nodeStack.empty())
	{
		currNode = nodeStack.top();
		nodeStack.pop();
		currSizeChunks = 1 << currNode->m_lod;

		// if our position is in range of this lod chunk for current lod
		float lodDist = ((CHUNK_LOD_RADIUS + 0.5f) * currSizeChunks) * CHUNK_UNIT_SIZE;
		posToChunkCenter = currNode->m_centerPos - position;
		int maxIndex = GetMaxVectorIndex(glm::abs(posToChunkCenter));
		float maxDistance = abs(posToChunkCenter[maxIndex]);
		if (currNode->m_lod > 0 && maxDistance < lodDist)
		{
			if (currNode->m_children[0] != nullptr && HasFinishedSubtree(currNode))
			{
				delete currNode->m_chunk;
				currNode->m_chunk = nullptr;
			}
			if (currNode->m_children[0] == nullptr)
			{
				for (uint i = 0; i < 8; i++)
				{
					float childOffsetScale = (1 << (currNode->m_lod - 1)) * CHUNK_UNIT_SIZE;
					glm::vec3 centerPos = currNode->m_centerPos + s_centerOctreeOffsets[i] * childOffsetScale;
					currNode->m_children[i] = std::make_shared<OctreeNode>(centerPos, currNode->m_lod - 1);
				}
			}
			for (uint i = 0; i < 8; i++)
			{
				nodeStack.push(currNode->m_children[i]);
			}
		}
		// otherwise back out and process other nodes
		else
		{
			if (currNode->m_chunk == nullptr)
			{
				glm::vec3 chunkNodeOffset = glm::vec3(-currSizeChunks * 0.5f * CHUNK_UNIT_SIZE);
				Chunk* chunk = new Chunk(currNode->m_centerPos + chunkNodeOffset, currNode->m_lod);
				newChunks.push_back(chunk);
				currNode->m_chunk = chunk;
				if (currNode->m_lod != 0 && maxDistance > lodDist && maxDistance < lodDist * (1.0f + 1.0f / CHUNK_LOD_RADIUS))
				{
					chunk->SetNeedsLODSeam(BlockFace(maxIndex * 2 + (posToChunkCenter[maxIndex] >= 0 ? 0 : 1)));
				}
			}
			if (currNode->m_children[0] != nullptr)
			{
				if (currNode->m_chunk->IsDeletable() && HasFinishedSubtree(currNode))
				{
					ReleaseChildren(currNode);
				}
				else
				{
					AddChildrenToVector(currNode, leafChunks);
				}
			}
		}

		if (currNode->m_chunk)
		{
			leafChunks.push_back(currNode->m_chunk);
		}
	}
}

void Octree::Clear()
{
	ReleaseChildrenBlocking(m_root);
}

Chunk* Octree::GetChunkAtWorldPos(const glm::vec3& worldPos)
{
	std::shared_ptr<OctreeNode> currNode = m_root;
	while (currNode->m_children[0] != nullptr)
	{
		const glm::vec3 nodeSpacePos = worldPos - currNode->m_centerPos;
		int childIndex = GetChildIndex(nodeSpacePos);
		currNode = currNode->m_children[childIndex];
	}
	return currNode->m_chunk;
}

int Octree::GetChildIndex(const glm::vec3& positionInNode)
{
	return 0 
		| (positionInNode.z < 0 ? 1u : 0u) 
		| ((positionInNode.y < 0 ? 1u : 0u) << 1u) 
		| ((positionInNode.x < 0 ? 1u : 0u) << 2u);
}

//void Octree::CreateChildren(std::shared_ptr<OctreeNode> parent)
//{
//	float offsetScale = (1 << (parent->m_lod - 1)) * CHUNK_UNIT_SIZE;
//	for (uint i = 0; i < 8; i++)
//	{
//		glm::vec3 centerPos = parent->m_centerPos + s_centerOctreeOffsets[i] * offsetScale;
//		Chunk* chunk = m_memPool.New();
//		chunk->CreateResources(centerPos, parent->m_lod - 1);
//		parent->m_children[i] = std::make_shared<OctreeNode>(chunk, centerPos, parent->m_lod - 1);
//	}
//}

bool Octree::ReleaseChildren(std::shared_ptr<OctreeNode> node)
{
	//ZoneScoped;
	if (node->m_children[0] == nullptr)
	{
		return (node->m_chunk == nullptr || node->m_chunk->IsDeletable());
	}
	for (uint i = 0; i < 8; i++)
	{
		if (!ReleaseChildren(node->m_children[i]))
			return false;
	}
	bool deletable = true;
	for (uint i = 0; i < 8; i++)
	{
		if (node->m_children[i]->m_chunk && !node->m_children[i]->m_chunk->IsDeletable())
		{
			deletable = false;
			return false;
		}
	}
	if (deletable)
	{
		for (uint i = 0; i < 8; i++)
		{
			delete node->m_children[i]->m_chunk;
			node->m_children[i]->m_chunk = nullptr;
			node->m_children[i] = nullptr;
		}
	}
	return true;
}

bool Octree::ReleaseChildrenBlocking(std::shared_ptr<OctreeNode> node)
{
	if (node->m_chunk)
	{
		while (!node->m_chunk->IsDeletable())
			continue;
		delete node->m_chunk;
		node->m_chunk = nullptr;
	}
	if (node->m_children[0] == nullptr)
	{
		return true;
	}
	for (uint i = 0; i < 8; i++)
	{
		if (!ReleaseChildren(node->m_children[i]))
			return false;
	}
	for (uint i = 0; i < 8; i++)
	{
		if (node->m_children[i]->m_chunk)
		{
			while (!node->m_children[i]->m_chunk->IsDeletable())
				continue;
			delete node->m_children[i]->m_chunk;
			node->m_children[i]->m_chunk = nullptr;
		}
		node->m_children[i] = nullptr;
	}
	return true;
}

void Octree::AddChildrenToVector(std::shared_ptr<OctreeNode> node, std::vector<Chunk*>& leafChunks)
{
	ZoneScoped;
	if (node->m_children[0] == nullptr)
		return;

	std::stack<std::shared_ptr<OctreeNode>> nodeStack;
	std::shared_ptr<OctreeNode> currNode = node;
	for (uint i = 0; i < 8; i++)
	{
		nodeStack.push(currNode->m_children[i]);
	}

	while (!nodeStack.empty())
	{
		currNode = nodeStack.top();
		nodeStack.pop();

		if (currNode->m_children[0] != nullptr)
		{
			for (uint i = 0; i < 8; i++)
			{
				nodeStack.push(currNode->m_children[i]);
			}
		}
		if (currNode->m_chunk)
		{
			leafChunks.push_back(currNode->m_chunk);
		}
	}
}

bool Octree::HasFinishedSubtree(std::shared_ptr<OctreeNode> node)
{
	//ZoneScoped;
	if (node->m_chunk && node->m_chunk->IsDeletable())
	{
		if (node->m_children[0] == nullptr)
		{
			return true;
		}
		else
		{
			for (uint i = 0; i < 8; i++)
			{
				if (!HasFinishedSubtree(node->m_children[i]))
					return false;
			}
		}
	}
	else
	{
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
OctreeNode::OctreeNode(Chunk* chunk, const glm::vec3& centerPos, uint lod)
{
	m_chunk = chunk;
	m_centerPos = centerPos;
	m_lod = lod;
}

OctreeNode::OctreeNode(const glm::vec3& centerPos, uint lod)
{
	m_chunk = nullptr;
	m_centerPos = centerPos;
	m_lod = lod;
}
