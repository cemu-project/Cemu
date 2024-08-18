#include "Fiber.h"
#include <Windows.h>

thread_local Fiber* sCurrentFiber{};

Fiber::Fiber(void(*FiberEntryPoint)(void* userParam), void* userParam, void* privateData) : m_privateData(privateData)
{
	HANDLE fiberHandle = CreateFiber(2 * 1024 * 1024, (LPFIBER_START_ROUTINE)FiberEntryPoint, userParam);
	this->m_implData = (void*)fiberHandle;
}

Fiber::Fiber(void* privateData) : m_privateData(privateData)
{
	this->m_implData = (void*)ConvertThreadToFiber(nullptr);
	this->m_stackPtr = nullptr;
}

Fiber::~Fiber()
{
	DeleteFiber((HANDLE)m_implData);
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
	SwitchToFiber((HANDLE)targetFiber.m_implData);
}

void* Fiber::GetFiberPrivateData()
{
	return sCurrentFiber->m_privateData;
}
