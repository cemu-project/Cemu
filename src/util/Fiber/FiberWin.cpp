#include "FiberWin.h"

thread_local Fiber* sCurrentFiber{};

Fiber::Fiber(void(*FiberEntryPoint)(void* userParam), void* userParam, void* privateData) : m_privateData(privateData)
{
	m_handle = CreateFiber(2 * 1024 * 1024, (LPFIBER_START_ROUTINE)FiberEntryPoint, userParam);
}

Fiber::Fiber(void* privateData) : m_privateData(privateData)
{
	m_handle = ConvertThreadToFiber(nullptr);
}

Fiber::~Fiber()
{
	DeleteFiber(m_handle);
}

Fiber* Fiber::PrepareCurrentThread(void* privateData)
{
	cemu_assert_debug(sCurrentFiber == nullptr); // thread already prepared
	Fiber* currentFiber = new Fiber(privateData);
	sCurrentFiber = currentFiber;
	return currentFiber;
}

void Fiber::Switch(Fiber& targetFiber)
{
	sCurrentFiber = &targetFiber;
	SwitchToFiber(targetFiber.m_handle);
}

void* Fiber::GetFiberPrivateData()
{
	return sCurrentFiber->m_privateData;
}
