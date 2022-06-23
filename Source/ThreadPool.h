// very naive approach to a thread pool. loosely adopted from C++ concurrency in action

#include <atomic>
#include <concurrent_priority_queue.h> // this might require some specific windows sdk version? 
#include <vector>
#include <thread>
#include <functional>

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
	unsigned id;
	std::function<void()> func;
	JobPriority priority;
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
				m_threads.push_back(std::thread(&ThreadPool::WorkerThread, this));
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

	int GetSize()
	{
		return m_workQueue.size();
	}

private:
	std::atomic_bool m_done;
	Concurrency::concurrent_priority_queue<Job, JobCmp> m_workQueue;
	std::vector<std::thread> m_threads;
	unsigned jobCount = 0;

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