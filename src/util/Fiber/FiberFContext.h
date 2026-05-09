#pragma once

#include <boost/context/detail/fcontext.hpp>

class Fiber
{
  public:
	Fiber(void (*FiberEntryPoint)(void* userParam), void* userParam, void* privateData);
	~Fiber();

	static Fiber* PrepareCurrentThread(void* privateData = nullptr);
	static void Switch(Fiber& targetFiber);
	static void* GetFiberPrivateData();

  private:
	static void Start(boost::context::detail::transfer_t transfer);
	Fiber(void* privateData); // fiber from current thread

	boost::context::detail::fcontext_t m_context{};

	void (*m_entryPoint)(void* userParam){};
	void* m_userParam{};

	void* m_privateData;

	void* m_stackPtr{};

	Fiber* m_prevFiber{};
};