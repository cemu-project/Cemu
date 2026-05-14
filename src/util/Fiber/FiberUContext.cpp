#include "FiberUContext.h"

thread_local Fiber* sCurrentFiber{};

Fiber::Fiber(void(*FiberEntryPoint)(void* userParam), void* userParam, void* privateData) : m_privateData(privateData)
{
	m_ctx = (ucontext_t*)malloc(sizeof(ucontext_t));
	
	const size_t stackSize = 2 * 1024 * 1024;
	m_stackPtr = malloc(stackSize);

	getcontext(m_ctx);
	m_ctx->uc_stack.ss_sp = m_stackPtr;
	m_ctx->uc_stack.ss_size = stackSize;
	m_ctx->uc_link = &m_ctx[0];
#ifdef __arm64__
	// https://www.man7.org/linux/man-pages/man3/makecontext.3.html#NOTES
	makecontext(m_ctx, (void(*)())FiberEntryPoint, 2, (uint64) userParam >> 32, userParam);
#else
	makecontext(m_ctx, (void(*)())FiberEntryPoint, 1, userParam);
#endif
}

Fiber::Fiber(void* privateData) : m_privateData(privateData)
{
	m_ctx = (ucontext_t*)malloc(sizeof(ucontext_t));
	getcontext(m_ctx);
	m_stackPtr = nullptr;
}

Fiber::~Fiber()
{
	if(m_stackPtr)
		free(m_stackPtr);
	free(m_ctx);
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
	swapcontext(leavingFiber->m_ctx, targetFiber.m_ctx);
	std::atomic_thread_fence(std::memory_order_seq_cst);
}

void* Fiber::GetFiberPrivateData()
{
	return sCurrentFiber->m_privateData;
}
