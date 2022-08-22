#include <thread>

class ThreadPool
{
public:
	template<class TFunction, class... TArgs>
	static void FireAndForget(TFunction&& f, TArgs&&... args)
	{
		// todo - find a way to use std::async here so we can utilize thread pooling?
		std::thread t(std::forward<TFunction>(f), std::forward<TArgs>(args)...);
		t.detach();
	}


};