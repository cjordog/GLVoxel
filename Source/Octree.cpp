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

Octree::Octree()
{
	// TODO:: create chunk here.
	m_centerPos = glm::vec3(0);
	m_size = 1024;
	m_maxDepth = std::max(10, 1); //log2(size) - 1
	m_root = std::make_shared<OctreeNode>(nullptr, m_centerPos, m_maxDepth);
}

// TODO:: condense empty chunks together?
void Octree::GenerateFromPosition(glm::vec3 position, std::vector<Chunk*>& newChunks, std::vector<Chunk*>& leafChunks)
{
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
		if ((abs(currNode->m_centerPos.x - position.x) / CHUNK_UNIT_SIZE) < (currSizeChunks / 2 + 2 * currSizeChunks) &&
			(abs(currNode->m_centerPos.y - position.y) / CHUNK_UNIT_SIZE) < (currSizeChunks / 2 + 2 * currSizeChunks) &&
			(abs(currNode->m_centerPos.z - position.z) / CHUNK_UNIT_SIZE) < (currSizeChunks / 2 + 2 * currSizeChunks))
		{
			if (currNode->m_children[0] == nullptr)
			{
				//CreateChildren(currNode);
				float offsetScale = (1 << (currNode->m_lod - 1)) * CHUNK_UNIT_SIZE;
				for (uint i = 0; i < 8; i++)
				{
					// WE CREATE THESE CHUNKS AND THEN POTENTIALLY JUST DELETE THEM RIGHT AWAY
					// s_octreeOffsets can actually hold offsets to actual corner.
					glm::vec3 pos = currNode->m_centerPos + s_centerOctreeOffsets[i] * offsetScale - glm::vec3(offsetScale * 0.5f);
					//Chunk* chunk = m_memPool.New();
					//chunk->CreateResources(centerPos, currNode->m_lod - 1);
					Chunk* chunk = new Chunk(pos / float(CHUNK_UNIT_SIZE), currNode->m_lod - 1);
					currNode->m_children[i] = std::make_shared<OctreeNode>(chunk, pos, currNode->m_lod - 1);
					// testing with just lod 0 right now
					//if (currNode->m_children[i]->m_lod == 0)
					{
						newChunks.push_back(chunk);
					}
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
			else if(currNode->m_children[0] != nullptr)
			{
				for (uint i = 0; i < 8; i++)
				{
					leafChunks.push_back(currNode->m_children[i]->m_chunk);
				}
			}
			if (currNode->m_children[0]->m_chunk && currNode->m_chunk && currNode->m_chunk->IsDeletable())
			{
				// maybe dont release these yet? will cause holes while smaller chunks generate, blegh
				//currNode->m_chunk->ReleaseResources();
				//m_memPool.Free(currNode->m_chunk);
				delete currNode->m_chunk;
				currNode->m_chunk = nullptr;
			}
		}
		// otherwise back out and process other nodes
		else
		{
			// if were totally out of range of this lod, we shouldnt have children
			if (currNode->m_children[0] != nullptr)
			{
				if (ReleaseChildren(currNode) && currNode->m_chunk == nullptr)
				{
					// check math here
					currNode->m_chunk = new Chunk(currNode->m_centerPos - glm::vec3(currSizeChunks * 0.5f), currNode->m_lod);
					newChunks.push_back(currNode->m_chunk);
					//currNode->m_chunk = m_memPool.New();
					//currNode->m_chunk->CreateResources(currNode->m_centerPos, currNode->m_lod);
					//chunksToGenerate.push_back(currNode->m_chunk);
				}
			}
			if (currNode->m_chunk)
			{
				leafChunks.push_back(currNode->m_chunk);
			}
		}
	}
}

void Octree::GenerateFromPosition2(glm::vec3 position, std::vector<Chunk*>& newChunks, std::vector<Chunk*>& leafChunks)
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
		if ((abs(currNode->m_centerPos.x - position.x / CHUNK_UNIT_SIZE)) < (currSizeChunks / 2.0f + 2 * currSizeChunks) &&
			(abs(currNode->m_centerPos.y - position.y / CHUNK_UNIT_SIZE)) < (currSizeChunks / 2.0f + 2 * currSizeChunks) &&
			(abs(currNode->m_centerPos.z - position.z / CHUNK_UNIT_SIZE)) < (currSizeChunks / 2.0f + 2 * currSizeChunks))
		{
			if (currNode->m_children[0] == nullptr)
			{
				//CreateChildren(currNode);
 				float offsetScale = (1 << (currNode->m_lod - 1));
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
				delete currNode->m_chunk;
				currNode->m_chunk = nullptr;
			}
		}
		// otherwise back out and process other nodes
		else
		{
			// if were totally out of range of this lod, we shouldnt have children
			if (currNode->m_children[0] != nullptr)
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
	// TODO:: can most certainly do this in a single combined pass with the above.
	// challenges are making sure we dont create chunks and then immediately destroy them as we descend the tree, and what to do if ReleaseChildren false
	nodeStack.push(m_root);
	while (!nodeStack.empty())
	{
		currNode = nodeStack.top();
		nodeStack.pop();
		currSizeChunks = 1 << currNode->m_lod;
		glm::vec3 offset = glm::vec3(-currSizeChunks * 0.5f);

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

//int Octree::GetChildIndex(const glm::vec3& octreePos)
//{
//	return 0 | (octreePos.x >= 0 ? 1u : 0u) | ((octreePos.y >= 0 ? 1u : 0u) << 1u) | ((octreePos.z >= 0 ? 1u : 0u) << 2u);
//}

void Octree::CreateChildren(std::shared_ptr<OctreeNode> parent)
{
	float offsetScale = (1 << (parent->m_lod - 1)) * CHUNK_UNIT_SIZE;
	for (uint i = 0; i < 8; i++)
	{
		glm::vec3 centerPos = parent->m_centerPos + s_centerOctreeOffsets[i] * offsetScale;
		Chunk* chunk = m_memPool.New();
		chunk->CreateResources(centerPos, parent->m_lod - 1);
		parent->m_children[i] = std::make_shared<OctreeNode>(chunk, centerPos, parent->m_lod - 1);
	}
}

bool Octree::ReleaseChildren(std::shared_ptr<OctreeNode> node)
{
	// TODO:: recursively go down children and release all resources down this branch
	// make sure that these nodes have m_chunk->IsDeletable()

	//std::stack<std::shared_ptr<OctreeNode>> nodeStack;
	//nodeStack.push(parent);
	//std::shared_ptr<OctreeNode> currNode;
	//while (!nodeStack.empty())
	//{
	//	currNode = nodeStack.top();
	//	nodeStack.pop();
	//	std::shared_ptr<OctreeNode> child = parent->m_children[0];
	//	if (child != nullptr)
	//	{
	//		if (currNode->m_lod > 0)
	//		{
	//			for (uint i = 0; i < 8; i++)
	//			{
	//				nodeStack.push(currNode->m_children[i]);
	//			}
	//		}
	//	}
	//}
	
	// covers lod 0 too since children are always null
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
			//node->m_children[i]->m_chunk->ReleaseResources();
			//m_memPool.Free(node->m_children[i]->m_chunk);
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
