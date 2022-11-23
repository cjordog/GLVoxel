// very naive approach to a thread pool. loosely adopted from C++ concurrency in action

#include <atomic>
//#include <concurrent_priority_queue.h> // this might require some specific windows sdk version? 
#include <queue>
#include <vector>
#include <thread>
#include <functional>
#include <unordered_map>
#include <mutex>

#include "RenderSettings.h"

enum JobPriority : unsigned
{
	Priority_Min = 0,
	Priority_Low,
	Priority_Med,
	Priority_High,
	Priority_Max,

	Num_Priorities,
};

struct Job
{
	unsigned id = 0;
	std::function<void()> func;
	JobPriority priority = JobPriority::Priority_Min;
};

struct JobCmp {
	bool operator()(const Job& lhs, const Job& rhs) const {
		return lhs.priority < rhs.priority;
	}
};

class ThreadPool
{
public:
	ThreadPool()
		: m_done(false)
	{
		std::unique_lock lock(queueMutex);
		RenderSettings& r = RenderSettings::Get();
		if (!r.mtEnabled)
			return;
		const unsigned threadCount = std::thread::hardware_concurrency();
		m_numThreads = threadCount;
		m_working = new std::atomic_bool[threadCount];
		try
		{
			m_threadIDs.reserve(threadCount);
			for (unsigned i = 0; i < threadCount; i++)
			{
				std::thread t(&ThreadPool::WorkerThread, this, i);
				m_threadIDs[t.get_id()] = i;
				m_threads.push_back(std::move(t));
			}
		}
		catch (...)
		{
			m_done = true;
			throw;
		}
	}

	~ThreadPool()
	{
		std::unique_lock lock(queueMutex);
		m_done = true;
		// this clears the queue
		std::priority_queue<Job, std::vector<Job>, JobCmp>().swap(m_workQueue);
		for (auto& thread : m_threads)
			thread.join();
		delete[] m_working;
	}

	template<typename FunctionType>
	void Submit(FunctionType t, JobPriority prio)
	{
		queueMutex.lock();
		m_workQueue.push({ jobCount++, std::function<void()>(t), prio });
		size_t workQueueSize = m_workQueue.size();
		queueMutex.unlock();
		if (workQueueSize == 1)
		{
			cv.notify_one();
		}
		else if (workQueueSize > 1)
		{
			cv.notify_all();
		}
	}

	//void Submit(std::function<void()> t, JobPriority prio)
	//{
	//	m_workQueue.push({ jobCount++, t, prio });
	//}

	bool GetPoolJob(Job& j)
	{
		std::unique_lock lock(queueMutex);
		if (!m_workQueue.empty())
		{
			j = m_workQueue.top();
			m_workQueue.pop();
			return true;
		}
		return false;
	}

	void ClearJobPool()
	{
		std::unique_lock lock(queueMutex);
		std::priority_queue<Job, std::vector<Job>, JobCmp>().swap(m_workQueue);
	}

	void WaitForAllThreadsFinished()
	{
		std::unique_lock<std::mutex> lock(queueMutex);
		while (1)
		{
			bool success = true;
			for (int i = 0; i < m_numThreads; i++)
			{
				if (m_working[i])
				{
					success = false;
					break;
				}
			}
			if (success)
				break;
		}
	}

	int GetSize()
	{
		std::unique_lock<std::mutex> lock(queueMutex);
		return m_workQueue.size();
	}

	const int GetNumThreads() const
	{
		return m_numThreads;
	}

	std::unordered_map<std::thread::id, int>& GetThreadIDs()
	{
		return m_threadIDs;
	}

private:
	std::atomic_bool m_done = false;
	std::atomic_bool* m_working = nullptr;
	//Concurrency::concurrent_priority_queue<Job, JobCmp> m_workQueue;
	std::priority_queue<Job, std::vector<Job>, JobCmp> m_workQueue;
	std::vector<std::thread> m_threads;
	unsigned jobCount = 0;
	std::unordered_map<std::thread::id, int> m_threadIDs;
	int m_numThreads = 0;
	std::condition_variable cv;
	std::mutex cvMutex;
	std::mutex queueMutex;

	void WorkerThread(int threadID)
	{
		while (!m_done)
		{
			queueMutex.lock();
			if (!m_workQueue.empty())
			{
				Job j = m_workQueue.top();
				m_workQueue.pop();
				queueMutex.unlock();
				// is this horrible?
				m_working[threadID] = true;
				j.func();
				m_working[threadID] = false;
			}
			else
			{
				queueMutex.unlock();
				std::unique_lock lock(cvMutex);
				cv.wait(lock, [&] {return m_workQueue.size() > 0; });
			}
		}
		return;
	}
};