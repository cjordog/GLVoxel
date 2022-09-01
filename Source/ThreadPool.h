// very naive approach to a thread pool. loosely adopted from C++ concurrency in action

#include <atomic>
#include <concurrent_priority_queue.h> // this might require some specific windows sdk version? 
#include <vector>
#include <thread>
#include <functional>
#include <unordered_map>

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
		return lhs.priority > rhs.priority;
	}
};

class ThreadPool
{
public:
	ThreadPool()
		: m_done(false)
	{
		RenderSettings& r = RenderSettings::Get();
		//r.mtEnabled = false;
		if (!r.mtEnabled)
			return;
		const unsigned threadCount = std::thread::hardware_concurrency();
		try
		{
			for (unsigned i = 0; i < threadCount; i++)
			{
				std::thread t(&ThreadPool::WorkerThread, this);
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
		m_done = true;
	}

	template<typename FunctionType>
	void Submit(FunctionType t, JobPriority prio)
	{
		m_workQueue.push({ jobCount++, std::function<void()>(t), prio });
	}

	bool GetPoolJob(Job& j)
	{
		if (m_workQueue.try_pop(j))
			return true;
		return false;
	}

	int GetSize() const
	{
		return m_workQueue.size();
	}

	const int GetNumThreads() const
	{
		return m_threads.size();
	}

	std::unordered_map<std::thread::id, int>* GetThreadIDs()
	{
		return &m_threadIDs;
	}

private:
	std::atomic_bool m_done;
	Concurrency::concurrent_priority_queue<Job, JobCmp> m_workQueue;
	std::vector<std::thread> m_threads;
	unsigned jobCount = 0;
	std::unordered_map<std::thread::id, int> m_threadIDs;

	void WorkerThread()
	{
		while (!m_done)
		{
			Job j;
			if (m_workQueue.try_pop(j))
			{
				j.func();
			}
			else
			{
				std::this_thread::yield();
			}
		}
	}
};

//ThreadPool g_threadPool;