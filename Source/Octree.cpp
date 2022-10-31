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

const float CHUNK_LOD_RADIUS = 3;
Octree::Octree()
{
	m_centerPos = glm::vec3(0);
	m_size = 1024;
#ifdef DEBUG
	const int numLODs = 4;
#else
	const int numLODs = 8;
#endif
	m_maxDepth = std::max(numLODs, 1); //log2(size) - 1
	m_root = std::make_shared<OctreeNode>(nullptr, m_centerPos, m_maxDepth);
}

void Octree::GenerateFromPosition(glm::vec3 position, std::vector<Chunk*>& newChunks, std::vector<Chunk*>& leafChunks)
{
	ZoneScoped;
	//chunksToGenerate.reserve(128);
	std::stack<std::shared_ptr<OctreeNode>> nodeStack;
	int currSizeChunks = m_size;
	std::shared_ptr<OctreeNode> currNode;
	nodeStack.push(m_root);
	while (!nodeStack.empty())
	{
		currNode = nodeStack.top();
		nodeStack.pop();
		currSizeChunks = 1 << currNode->m_lod;

		// if our position is in range of this lod chunk for current lod
		float lodDist = ((CHUNK_LOD_RADIUS + 0.5f) * currSizeChunks) * CHUNK_UNIT_SIZE;
		if (currNode->m_lod > 0 && 
			(abs(currNode->m_centerPos.x - position.x)) < lodDist &&
			(abs(currNode->m_centerPos.y - position.y)) < lodDist &&
			(abs(currNode->m_centerPos.z - position.z)) < lodDist)
		{
			if (currNode->m_chunk && currNode->m_chunk->IsDeletable()/* && HasFinishedSubtree(currNode)*/)
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
			}
			if (currNode->m_lod > 0)
			{
				ReleaseChildren(currNode);
			}
		}

		if (currNode->m_chunk)
		{
			leafChunks.push_back(currNode->m_chunk);
		}
	}
}

void Octree::GenerateFromPosition2(glm::vec3 position, std::vector<Chunk*>& newChunks, std::vector<Chunk*>& leafChunks)
{
	ZoneNamed(GenerateFromPosition1, true);
	//chunksToGenerate.reserve(128);
	std::stack<std::shared_ptr<OctreeNode>> nodeStack;
	int currSizeChunks = m_size;
	std::shared_ptr<OctreeNode> currNode;
	nodeStack.push(m_root);
	while (!nodeStack.empty())
	{
		currNode = nodeStack.top();
		nodeStack.pop();
		currSizeChunks = 1 << currNode->m_lod;

		// if our position is in range of this lod chunk for current lod
		float lodDist = (currSizeChunks / 2.0f + 3 * currSizeChunks) * CHUNK_UNIT_SIZE;
		if ((abs(currNode->m_centerPos.x - position.x)) < lodDist &&
			(abs(currNode->m_centerPos.y - position.y)) < lodDist &&
			(abs(currNode->m_centerPos.z - position.z)) < lodDist)
		{
			if (currNode->m_children[0] == nullptr)
			{
				//CreateChildren(currNode);
 				float offsetScale = (1 << (currNode->m_lod - 1)) * CHUNK_UNIT_SIZE;
				for (uint i = 0; i < 8; i++)
				{
					// any reason this needs to be center pos but chunks are bottom left corner?
					// dont create chunks yet because they could be destroyed in a later iteration
					glm::vec3 centerPos = currNode->m_centerPos + s_centerOctreeOffsets[i] * offsetScale;
					currNode->m_children[i] = std::make_shared<OctreeNode>(centerPos, currNode->m_lod - 1);
				}
			}
			// never push leaf nodes to process stack, they get processed as children of lod 1 in above loop
			if (currNode->m_lod > 1)
			{
				for (uint i = 0; i < 8; i++)
				{
					nodeStack.push(currNode->m_children[i]);
				}
			}
			// if we have children, but also a chunk. chunk is old lod. get it outta here
			if (currNode->m_chunk && currNode->m_chunk->IsDeletable())
			{
				bool deletable = true;
				for (uint i = 0; i < 8; i++)
				{
					// checking !currNode->m_children[i]->m_chunk might cause mem leaks?
					if (!currNode->m_children[i]->m_chunk || !currNode->m_children[i]->m_chunk->IsDeletable())
					{
						deletable = false;
					}
				}
				if (deletable)
				{
					delete currNode->m_chunk;
					currNode->m_chunk = nullptr;
				}
			}
		}
		// otherwise back out and process other nodes
		else
		{
			// if were totally out of range of this lod, we shouldnt have children
			if (currNode->m_children[0] != nullptr && currNode->m_chunk && currNode->m_chunk->IsDeletable())
			{
				// if false, we need to iterate subtree and add leaf nodes to leafChunks array
				ReleaseChildren(currNode);
			}
		}

		if (currNode->m_children[0] == nullptr)
		{
			// add children to leafChunks. if we werent using 2 passes
		}
	}

	ZoneNamed(GenerateFromPosition2, true);

	// TODO:: can most certainly do this in a single combined pass with the above.
	// challenges are making sure we dont create chunks and then immediately destroy them as we descend the tree, and what to do if ReleaseChildren false
	nodeStack.push(m_root);
	while (!nodeStack.empty())
	{
		currNode = nodeStack.top();
		nodeStack.pop();
		currSizeChunks = 1 << currNode->m_lod;
		glm::vec3 offset = glm::vec3(-currSizeChunks * 0.5f * CHUNK_UNIT_SIZE);

		if (currNode->m_children[0] != nullptr)
		{
			if (currNode->m_lod <= 1)
			{
				for (int i = 0; i < 8; i++)
				{
					Chunk* childChunk = currNode->m_children[i]->m_chunk;
					if (childChunk == nullptr)
					{
						// childs lod level, divide offset by half
						childChunk = new Chunk(currNode->m_children[i]->m_centerPos + offset * 0.5f, currNode->m_children[i]->m_lod);
						newChunks.push_back(childChunk);
						currNode->m_children[i]->m_chunk = childChunk;
					}
					leafChunks.push_back(childChunk);
				}
			}
			else
			{
				for (int i = 0; i < 8; i++)
				{
					nodeStack.push(currNode->m_children[i]);
				}
			}
		}
		else
		{
			Chunk* chunk = currNode->m_chunk;
			if (chunk == nullptr)
			{
				chunk = new Chunk(currNode->m_centerPos + offset, currNode->m_lod);
				newChunks.push_back(chunk);
				currNode->m_chunk = chunk;
			}
			leafChunks.push_back(chunk);
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
	ZoneScoped;
	if (node->m_children[0] == nullptr)
	{
		return (node->m_chunk == nullptr || node->m_chunk->IsDeletable());
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
			delete node->m_children[i]->m_chunk;
			node->m_children[i]->m_chunk = nullptr;
		}
		node->m_children[i] = nullptr;
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
