#pragma once

#include <mutex>
#include <memory>

template<typename T>
class SingletonClass
{
public:
	static T* getInstance()
	{
		static T instance;
		return &instance;
	}

protected:
	SingletonClass() = default;
};

template<typename T>
class SingletonRef
{
public:
	/*static std::shared_ptr<T> getInstance() C++20 only
	{
		static std::atomic<std::weak_ptr<T>> s_instance;
		std::shared_ptr<T> result;
		s_instance.compare_exchange_weak(result, std::make_shared<T>());
		return result;
	}*/
	
	static std::shared_ptr<T> getInstance()
	{
		std::scoped_lock lock(s_mutex);
		
		auto result = s_instance.lock();
		if(!result)
		{
			result = std::make_shared<T>();
			s_instance = result;
		}
		
		return result;
	}

protected:
	SingletonRef() = default;

private:
	static inline std::weak_ptr<T> s_instance;
	static inline std::mutex s_mutex;
};