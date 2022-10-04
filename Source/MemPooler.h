#pragma once

#include "Common.h"
#include <vector>
#include <queue>

template <typename T> class MemPooler
{
public:
	MemPooler() 
	{
		m_reclaimedItems = std::queue<T*>();
		// this is just.. awful
		m_data = (T*)malloc(sizeof(T) * 20000);
	}

	T* New()
	{
		if (maxIndex >= 4096)
			assert(false);
		if (m_reclaimedItems.size())
		{
			T* mem = m_reclaimedItems.front();
			m_reclaimedItems.pop();
			return mem;
		}
		else
		{
			return &m_data[maxIndex++];
		}
	}

	void Free(T* obj)
	{
		// we should probably call some sort of free on the item for things like vertex arrays on Chunks? will happen when it gets recycled but..
		m_reclaimedItems.push(obj);
		m_hasReclaimedItems = true;
	}

private:
	T* m_data;
	std::queue<T*> m_reclaimedItems;
	int maxIndex = 0;
	bool m_hasReclaimedItems = false;
};