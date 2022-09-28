#include "Octree.h"
#include <stack>

static glm::vec3 s_octreeOffsets[8] =
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

Octree::Octree()
{
	// TODO:: create chunk here.
	m_centerPos = glm::vec3(0);
	m_size = 1024;
	m_maxDepth = 9; //log2(size) - 1
	m_root = std::make_shared<OctreeNode>(nullptr, m_centerPos, m_maxDepth);
	
	// can never actually see the chunk stored at m_root without being very far away from the actual octree
	CreateChildren(m_root);
}

// TODO:: condense empty chunks together?
void Octree::GenerateFromPosition(glm::vec3 position, std::vector<Chunk*>& chunksToGenerate)
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
					glm::vec3 centerPos = currNode->m_centerPos + s_octreeOffsets[i] * offsetScale;
					Chunk* chunk = m_memPool.New();
					chunksToGenerate.push_back(chunk);
					chunk->CreateResources(centerPos, currNode->m_lod - 1);
					// check lod level?
					currNode->m_children[i] = std::make_shared<OctreeNode>(chunk, centerPos, currNode->m_lod - 1);
				}
			}
			// never push leaf nodes to process stack
			if (currNode->m_lod > 0)
			{
				for (uint i = 0; i < 8; i++)
				{
					nodeStack.push(currNode->m_children[i]);
				}
			}
			if (currNode->m_chunk && currNode->m_chunk->GetChunkState() == Chunk::ChunkState::Done)
			{
				// maybe dont release these yet? will cause holes while smaller chunks generate, blegh
				currNode->m_chunk->ReleaseResources();
				m_memPool.Free(currNode->m_chunk);
				currNode->m_chunk = nullptr;
			}
		}
		// otherwise back out and process other nodes
		else
		{
			// if were totally out of range of this lod, we shouldnt have children
			if (currNode->m_children[0] != nullptr)
			{
				ReleaseChildren(currNode);
				if (currNode->m_chunk == nullptr)
				{
					currNode->m_chunk = m_memPool.New();
					currNode->m_chunk->CreateResources(currNode->m_centerPos, currNode->m_lod);
					chunksToGenerate.push_back(currNode->m_chunk);
				}
			}
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
		glm::vec3 centerPos = parent->m_centerPos + s_octreeOffsets[i] * offsetScale;
		Chunk* chunk = m_memPool.New();
		chunk->CreateResources(centerPos, parent->m_lod - 1);
		parent->m_children[i] = std::make_shared<OctreeNode>(chunk, centerPos, parent->m_lod - 1);
	}
}

bool Octree::ReleaseChildren(std::shared_ptr<OctreeNode> node)
{
	// TODO:: recursively go down children and release all resources down this branch
	// make sure that these nodes have m_chunk->GetChunkState() == Chunk::ChunkState::Done

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
		return (node->m_chunk->GetChunkState() == Chunk::ChunkState::Done);
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
			node->m_children[i]->m_chunk->ReleaseResources();
			m_memPool.Free(node->m_children[i]->m_chunk);
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