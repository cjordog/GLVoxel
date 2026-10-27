// very naive approach to a thread pool. loosely adopted from C++ concurrency in action
#pragma once

#include <atomic>
//#include <concurrent_priority_queue.h> // this might require some specific windows sdk version? 
#include <queue>
#include <vector>
#include <thread>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <algorithm>

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
	{
		RenderSettings& r = RenderSettings::Get();
		if (!r.mtEnabled)
			return;
		// hardware_concurrency can return 0 if it can't be determined
		const unsigned threadCount = std::max(1u, std::thread::hardware_concurrency());
		m_numThreads = threadCount;
		try
		{
			m_threadIDs.reserve(threadCount);
			for (unsigned i = 0; i < threadCount; i++)
			{
				std::thread t(&ThreadPool::WorkerThread, this);
				m_threadIDs[t.get_id()] = i;
				m_threads.push_back(std::move(t));
			}
		}
		catch (...)
		{
			Shutdown();
			throw;
		}
	}

	~ThreadPool()
	{
		Shutdown();
	}

	template<typename FunctionType>
	void Submit(FunctionType t, JobPriority prio)
	{
		{
			std::lock_guard lock(m_queueMutex);
			m_workQueue.push({ m_jobCount++, std::function<void()>(t), prio });
		}
		m_workAvailableCv.notify_one();
	}

	bool GetPoolJob(Job& j)
	{
		std::lock_guard lock(m_queueMutex);
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
		std::lock_guard lock(m_queueMutex);
		std::priority_queue<Job, std::vector<Job>, JobCmp>().swap(m_workQueue);
	}

	// blocks until no worker is running a job. jobs still in the queue are not waited on,
	// so call ClearJobPool first if you need everything to be idle.
	void WaitForAllThreadsFinished()
	{
		std::unique_lock lock(m_queueMutex);
		m_idleCv.wait(lock, [&] { return m_activeJobs == 0; });
	}

	int GetSize()
	{
		std::lock_guard lock(m_queueMutex);
		return int(m_workQueue.size());
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
	// everything below except m_threadIDs/m_threads is guarded by m_queueMutex
	bool m_done = false;
	unsigned m_activeJobs = 0;
	//Concurrency::concurrent_priority_queue<Job, JobCmp> m_workQueue;
	std::priority_queue<Job, std::vector<Job>, JobCmp> m_workQueue;
	std::vector<std::thread> m_threads;
	unsigned m_jobCount = 0;
	std::unordered_map<std::thread::id, int> m_threadIDs;
	int m_numThreads = 0;
	std::condition_variable m_workAvailableCv;
	std::condition_variable m_idleCv;
	std::mutex m_queueMutex;

	void Shutdown()
	{
		{
			std::lock_guard lock(m_queueMutex);
			m_done = true;
			// this clears the queue
			std::priority_queue<Job, std::vector<Job>, JobCmp>().swap(m_workQueue);
		}
		// wake every sleeping worker so it can see m_done. must not hold the lock while joining.
		m_workAvailableCv.notify_all();
		for (auto& thread : m_threads)
		{
			if (thread.joinable())
				thread.join();
		}
		m_threads.clear();
	}

	void WorkerThread()
	{
		while (true)
		{
			Job j;
			{
				std::unique_lock lock(m_queueMutex);
				m_workAvailableCv.wait(lock, [&] { return m_done || !m_workQueue.empty(); });
				if (m_done)
					return;
				j = m_workQueue.top();
				m_workQueue.pop();
				// counted as active in the same critical section as the pop, so
				// WaitForAllThreadsFinished can never see a dequeued-but-not-started job as idle
				m_activeJobs++;
			}

			j.func();

			{
				std::lock_guard lock(m_queueMutex);
				m_activeJobs--;
			}
			m_idleCv.notify_all();
		}
	}
};
