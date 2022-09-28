#include "MemPooler.h"
//
//template <typename T>
//MemPooler<T>::MemPooler()
//{
//	m_reclaimedItems = std::queue<T*>();
//	m_data = std::vector<T>();
//	m_data.reserve(512);
//}
//
//template <typename T>
//T* MemPooler<T>::New()
//{
//	if (m_hasReclaimedItems)
//	{
//		T* mem = m_reclaimedItems.front();
//		m_reclaimedItems.pop();
//		return mem;
//	}
//	else
//	{
//		return &m_data.push_back();
//	}
//}
//
//template <typename T>
//void MemPooler<T>::Free(T* obj) 
//{
//	// we should probably call some sort of free on the item for things like vertex arrays on Chunks? will happen when it gets recycled but..
//	m_reclaimedItems.push(obj);
//}
