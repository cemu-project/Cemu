#include "CafeStaticVar.h"
#include "Cafe/HW/MMU/MMU.h"
#include "util/ChunkedHeap/ChunkedHeap.h"

namespace
{
	uint32 AlignOffset(uint32 offset, uint32 alignment)
	{
		cemu_assert_debug((offset+alignment) >= offset);
		uint64 aligned = (offset + alignment - 1u) & ~(alignment - 1u);
		return static_cast<uint32>(aligned);
	}

	std::unique_ptr<VHeap> s_iosuStaticDataHeap{};
	std::set<CafeStaticData*> s_iosuStaticDataBlocks;
	std::mutex s_cafeStaticDataMutex;
}

CafeStaticVarBase::CafeStaticVarBase(CafeStaticData& owner, uint32 size, uint32 alignment)
	: m_owner(&owner), m_size(size), m_alignment(alignment)
{
	std::unique_lock _l(s_cafeStaticDataMutex);
	if (!owner.m_vars)
		owner.m_vars = new std::vector<CafeStaticVarBase*>();
	owner.m_vars->push_back(this);
}

CafeStaticVarBase::~CafeStaticVarBase()
{
	std::unique_lock _l(s_cafeStaticDataMutex);
	if (m_owner)
	{
		cemu_assert(!m_owner->IsAllocated());
		std::erase(*m_owner->m_vars, this);
	}
}

bool CafeStaticData::HasAnyAllocations()
{
	std::unique_lock _l(s_cafeStaticDataMutex);
	return !s_iosuStaticDataBlocks.empty();
}

MPTR coreinit_allocFromSysArea(uint32 size, uint32 alignment);
void coreinit_freeToSysArea(MPTR mem);

void CafeStaticData::Allocate()
{
	std::unique_lock _l(s_cafeStaticDataMutex);
	if (!m_vars)
		return; // empty
	// register this static block
	cemu_assert_debug(!s_iosuStaticDataBlocks.contains(this));
	s_iosuStaticDataBlocks.emplace(this);
	// calculate total allocation size
	uint32 size = 0;
	uint32 alignment = 32;
	for (auto& var : *m_vars)
	{
		size = AlignOffset(size, var->m_alignment);
		cemu_assert(var->m_size < 1024 * 1024 * 128); // sanity check size
		size += var->m_size;
		alignment = std::max(alignment, var->m_alignment);
	}
	cemu_assert_debug(size > 0); // zero sized variables are not allowed
	// get heap
	VHeap* heap = nullptr;
	if (m_mode == Mode::IOSU)
	{
		cemu_assert(mmuRange_IOSU_AREA.isMapped());
		if (!s_iosuStaticDataHeap)
			s_iosuStaticDataHeap = std::make_unique<VHeap>(mmuRange_IOSU_AREA.getPtr(), mmuRange_IOSU_AREA.getSize());
		heap = s_iosuStaticDataHeap.get();
		m_allocation = heap->alloc(size, alignment);
	}
	else if (m_mode == Mode::COS)
	{
		MPTR allocOffset = coreinit_allocFromSysArea(size, alignment);
		cemu_assert(allocOffset != MPTR_NULL);
		m_allocation = MEMPTR<void>(allocOffset).GetPtr();
	}
	cemu_assert(m_allocation);
	uint32 offset = 0;
	for (auto& var : *m_vars)
	{
		offset = AlignOffset(offset, var->m_alignment);
		var->m_memory = static_cast<uint8*>(m_allocation) + offset;
		offset += var->m_size;
	}
	for (auto& var : *m_vars)
		var->Initialize();
}

void CafeStaticData::Deallocate()
{
	std::unique_lock _l(s_cafeStaticDataMutex);
	if (!m_vars)
		return; // empty
	cemu_assert(m_allocation);
	for (auto& var : *m_vars)
		var->m_memory = nullptr;
	if (m_mode == Mode::IOSU)
	{
		s_iosuStaticDataHeap->free(m_allocation);
	}
	else // COS
	{
		coreinit_freeToSysArea(MEMPTR<void>(m_allocation).GetMPTR());
	}
	m_allocation = nullptr;
	s_iosuStaticDataBlocks.erase(this);
}

CafeStaticData::~CafeStaticData()
{
	std::unique_lock _l(s_cafeStaticDataMutex);
	for (auto& it : *m_vars)
		it->m_owner = nullptr;
	delete m_vars;
}