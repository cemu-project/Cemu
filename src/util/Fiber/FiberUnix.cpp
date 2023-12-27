#include "Fiber.h"
#if __ANDROID__
#include <libucontext/libucontext.h>
using _ucontext_t = libucontext_ucontext_t;
constexpr auto& swapcontext = libucontext_swapcontext;
constexpr auto& getcontext = libucontext_getcontext;
constexpr auto& makecontext = libucontext_makecontext;
#else
#include <ucontext.h>
using _ucontext_t = ucontext_t;
#endif
#include <atomic>

thread_local Fiber* sCurrentFiber{};

Fiber::Fiber(void(*FiberEntryPoint)(void* userParam), void* userParam, void* privateData) : m_privateData(privateData)
{
	_ucontext_t* ctx = (_ucontext_t*)malloc(sizeof(_ucontext_t));
	
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
	_ucontext_t* ctx = (_ucontext_t*)malloc(sizeof(_ucontext_t));
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
	std::atomic_thread_fence(std::memory_order_seq_cst);
	swapcontext((_ucontext_t*)(leavingFiber->m_implData), (_ucontext_t*)(targetFiber.m_implData));
	std::atomic_thread_fence(std::memory_order_seq_cst);
}

void* Fiber::GetFiberPrivateData()
{
	return sCurrentFiber->m_privateData;
}
