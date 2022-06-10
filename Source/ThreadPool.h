// very naive approach to a thread pool. loosely adopted from C++ concurrency in action

#include <atomic>
#include <concurrent_priority_queue.h> // this might require some specific windows sdk version? 
#include <vector>
#include <thread>
#include <functional>

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
	int id;
	std::function<void()> func;
	JobPriority priority;
};

struct JobCmp {
	bool operator()(const Job& lhs, const Job& rhs) const {
		return static_cast<std::underlying_type<JobPriority>::type>(lhs.priority) > static_cast<std::underlying_type<JobPriority>::type>(rhs.priority);
	}
};

class ThreadPool
{
public:
	ThreadPool()
		: m_done(false)
	{
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

private:
	std::atomic_bool m_done;
	Concurrency::concurrent_priority_queue<Job> m_workQueue;
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