#pragma once

#include "Common.h"
#include <vector>
#include <memory>
#include <mutex>

// hands out T's from fixed-size blocks. blocks are never reallocated or moved, so pointers
// returned by New() stay valid for the lifetime of the pool no matter how much it grows.
// (the previous std::vector backed version would invalidate every outstanding pointer if it
// ever had to grow past its reserve, and reserved ~1.1GB up front to avoid that.)
template <typename T> class MemPooler
{
public:
	explicit MemPooler(size_t elementsPerBlock)
		: m_blockSize(elementsPerBlock > 0 ? elementsPerBlock : 1)
	{
	}

	MemPooler(const MemPooler&) = delete;
	MemPooler& operator=(const MemPooler&) = delete;

	T* New()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_reclaimedItems.empty())
		{
			T* mem = m_reclaimedItems.back();
			m_reclaimedItems.pop_back();
			return mem;
		}

		if (m_blocks.empty() || m_nextInBlock == m_blockSize)
		{
			m_blocks.push_back(std::make_unique<T[]>(m_blockSize));
			m_nextInBlock = 0;
		}
		return &m_blocks.back()[m_nextInBlock++];
	}

	void Free(T* obj)
	{
		if (obj != nullptr)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_reclaimedItems.push_back(obj);
		}
	}

private:
	const size_t m_blockSize;
	size_t m_nextInBlock = 0;
	std::vector<std::unique_ptr<T[]>> m_blocks;
	std::vector<T*> m_reclaimedItems;
	std::mutex m_mutex;
};
