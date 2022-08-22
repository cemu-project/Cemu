#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

template <typename T> 
class ConcurrentQueue
{
public:
	ConcurrentQueue() = default;
	ConcurrentQueue(const ConcurrentQueue&) = delete;
	ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

	size_t push(const T& item)
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		m_queue.push(item);
		const size_t result = m_queue.size();
		mlock.unlock();
		m_condVar.notify_one();
		return result;
	}

	template<typename ... Args>
	size_t push(Args&& ... args)
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		m_queue.emplace(std::forward<Args>(args)...);
		const size_t result = m_queue.size();
		mlock.unlock();
		m_condVar.notify_one();
		return result;
	}

	T peek()
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		while (m_queue.empty())
		{
			m_condVar.wait(mlock);
		}

		return m_queue.front();
	}

	bool peek2(T& item)
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		if (m_queue.empty())
			return false;
		item = m_queue.front();
		m_queue.pop();
		return true;
	}

	T pop()
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		while (m_queue.empty())
		{
			m_condVar.wait(mlock);
		}

		auto val = m_queue.front();
		m_queue.pop();
		return val;
	}

	void pop(T& item)
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		while (m_queue.empty())
		{
			m_condVar.wait(mlock);
		}

		item = m_queue.front();
		m_queue.pop();
	}

	void clear()
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		while (!m_queue.empty())
			m_queue.pop();
	}

	size_t size()
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		return m_queue.size();
	}

	bool empty()
	{
		std::unique_lock<std::mutex> mlock(m_mutex);
		return m_queue.empty();
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_condVar;
	std::queue<T> m_queue;
};
