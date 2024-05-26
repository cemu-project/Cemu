
#include <boost/context/detail/fcontext.hpp>

#include "Fiber.h"

static constexpr size_t stackSize = 2 * 1024 * 1024;

struct FiberImpl
{
  bool isThread = false;
  void* userParam;
  void* stack;
  std::function<void(void*)> entryPoint;
  boost::context::detail::fcontext_t context{};
  FiberImpl* previousFiber;
  FiberImpl()
  {
    stack = malloc(stackSize);
  }
  static void start(boost::context::detail::transfer_t transfer)
  {
    FiberImpl* fiber = static_cast<FiberImpl*>(transfer.data);
    fiber->previousFiber->context = transfer.fctx;
    fiber->previousFiber = nullptr;
    fiber->entryPoint(fiber->userParam);
  }
  ~FiberImpl()
  {
    free(stack);
  }
};
thread_local Fiber* sCurrentFiber{};

Fiber::Fiber(void (*FiberEntryPoint)(void* userParam), void* userParam, void* privateData)
    : m_privateData(privateData)
{
  FiberImpl* impl = new FiberImpl;
  impl->entryPoint = FiberEntryPoint;
  impl->userParam = userParam;
  impl->context = boost::context::detail::make_fcontext(reinterpret_cast<uint8_t*>(impl->stack) + stackSize, stackSize, FiberImpl::start);
  m_implData = impl;
}

Fiber::Fiber(void* privateData)
    : m_privateData(privateData)
{
  m_implData = new FiberImpl;
}

Fiber::~Fiber()
{
  delete static_cast<FiberImpl*>(m_implData);
}

Fiber* Fiber::PrepareCurrentThread(void* privateData)
{
  sCurrentFiber = new Fiber(privateData);
  return sCurrentFiber;
}

void Fiber::Switch(Fiber& targetFiber)
{
  if (&targetFiber == sCurrentFiber)
  {
    return;
  }
  FiberImpl* _currentFiber = static_cast<FiberImpl*>(sCurrentFiber->m_implData);
  FiberImpl* _targetFiber = static_cast<FiberImpl*>(targetFiber.m_implData);
  _targetFiber->previousFiber = _currentFiber;
  sCurrentFiber = &targetFiber;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  auto transfer = boost::context::detail::jump_fcontext(_targetFiber->context, _targetFiber);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if (_currentFiber->previousFiber == nullptr)
  {
    return;
  }
  _currentFiber->previousFiber->context = transfer.fctx;
  _currentFiber->previousFiber->previousFiber = nullptr;
}

void* Fiber::GetFiberPrivateData()
{
  return sCurrentFiber->m_privateData;
}
