#include "Fiber.h"
#include <ucontext.h>

thread_local Fiber* sCurrentFiber{};

Fiber::Fiber(void(*FiberEntryPoint)(void* userParam), void* userParam, void* privateData) : m_privateData(privateData)
{
	ucontext_t* ctx = (ucontext_t*)malloc(sizeof(ucontext_t));
	
	const size_t stackSize = 2 * 1024 * 1024;
	m_stackPtr = malloc(stackSize);

	getcontext(ctx);
	ctx->uc_stack.ss_sp = m_stackPtr;
	ctx->uc_stack.ss_size = stackSize;
	ctx->uc_link = &ctx[0];
	makecontext(ctx, (void(*)())FiberEntryPoint, 1, userParam);
	this->m_implData = (void*)ctx;
}

Fiber::Fiber(void* privateData) : m_privateData(privateData)
{
	ucontext_t* ctx = (ucontext_t*)malloc(sizeof(ucontext_t));
	getcontext(ctx);
	this->m_implData = (void*)ctx;
	m_stackPtr = nullptr;
}

Fiber::~Fiber()
{
	if(m_stackPtr)
		free(m_stackPtr);
	free(m_implData);
}

Fiber* Fiber::PrepareCurrentThread(void* privateData)
{
	cemu_assert_debug(sCurrentFiber == nullptr);
    sCurrentFiber = new Fiber(privateData);
	return sCurrentFiber;
}

void Fiber::Switch(Fiber& targetFiber)
{
    Fiber* leavingFiber = sCurrentFiber;
    sCurrentFiber = &targetFiber;
	swapcontext((ucontext_t*)(leavingFiber->m_implData), (ucontext_t*)(targetFiber.m_implData));
}

void* Fiber::GetFiberPrivateData()
{
	return sCurrentFiber->m_privateData;
}
