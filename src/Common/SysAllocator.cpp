#include "SysAllocator.h"

void SysAllocatorContainer::Initialize()
{
	for (SysAllocatorBase* sysAlloc : m_sysAllocList)
	{
		sysAlloc->Initialize();
	}
}

void SysAllocatorContainer::PushSysAllocator(SysAllocatorBase* base)
{
	m_sysAllocList.push_back(base);
}

SysAllocatorContainer& SysAllocatorContainer::GetInstance()
{
	static SysAllocatorContainer s_instance;
	return s_instance;
}

SysAllocatorBase::SysAllocatorBase()
{
	SysAllocatorContainer::GetInstance().PushSysAllocator(this);
}