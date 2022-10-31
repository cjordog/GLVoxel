#pragma once

#include "Common.h"
#include <vector>
#include <queue>
#include <mutex>

template <typename T> class MemPooler
{
public:
	MemPooler(int elements) 
	{
		m_reclaimedItems = std::queue<T*>();
		m_data.reserve(elements);
		size = elements;
	}

	T* New()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_reclaimedItems.size())
		{
			T* mem = m_reclaimedItems.front();
			m_reclaimedItems.pop();
			return mem;
		}
		else
		{
			int cap = m_data.capacity();
			T* t = &m_data.emplace_back();
			assert(cap == m_data.capacity());
			return t;
		}
	}

	void Free(T* obj)
	{
		if (obj != nullptr)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_reclaimedItems.push(obj);
		}
	}

private:
	//T* m_data;
	std::vector<T> m_data;
	std::queue<T*> m_reclaimedItems;
	std::mutex m_mutex;
	int maxIndex = 0;
	int size = 0;
	bool m_hasReclaimedItems = false;
};