#pragma once

#include <Windows.h>

class Fiber
{
public:
	Fiber(void(*FiberEntryPoint)(void* userParam), void* userParam, void* privateData);
	~Fiber();

	static Fiber* PrepareCurrentThread(void* privateData = nullptr);
	static void Switch(Fiber& targetFiber);
	static void* GetFiberPrivateData();
private:
	Fiber(void* privateData); // fiber from current thread

	HANDLE m_handle{nullptr};
	void* m_privateData;
};