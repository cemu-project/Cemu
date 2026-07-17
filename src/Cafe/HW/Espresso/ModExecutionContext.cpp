#include "Cafe/HW/Espresso/ModExecutionContext.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
	bool ContainsPermission(ModMemoryPermission available, ModMemoryPermission requested)
	{
		return (static_cast<std::uint8_t>(available) & static_cast<std::uint8_t>(requested)) ==
			static_cast<std::uint8_t>(requested);
	}
}

ModExecutionContext::ModExecutionContext(std::uint64_t addressSpaceId, std::uint32_t generation,
	std::string principal, std::uint32_t virtualBase, std::uint32_t size)
	: m_addressSpaceId(addressSpaceId), m_generation(generation), m_principal(std::move(principal)),
	  m_virtualBase(virtualBase)
{
	if (addressSpaceId == 0 || generation == 0 || m_principal.empty() || size == 0 ||
		size > kMaximumAddressSpaceBytes || virtualBase % kPageSize != 0 || size % kPageSize != 0 ||
		virtualBase > std::numeric_limits<std::uint32_t>::max() - size)
	{
		m_stopped = true;
		m_fault.reason = ModFaultReason::InvalidMapping;
		return;
	}
	m_memory.resize(size);
	m_pages.resize(size / kPageSize, ModMemoryPermission::None);
	BeginFrame();
}

bool ModExecutionContext::ValidateRange(std::uint32_t address, std::size_t size,
	std::size_t& offset) const
{
	if (size == 0 || address < m_virtualBase)
		return false;
	offset = static_cast<std::size_t>(address - m_virtualBase);
	return offset <= m_memory.size() && size <= m_memory.size() - offset;
}

bool ModExecutionContext::HasPermission(std::uint32_t address, std::size_t size,
	ModMemoryPermission permission) const
{
	std::size_t offset{};
	if (!ValidateRange(address, size, offset))
		return false;
	const auto firstPage = offset / kPageSize;
	const auto lastPage = (offset + size - 1) / kPageSize;
	for (auto page = firstPage; page <= lastPage; ++page)
		if (!ContainsPermission(m_pages[page], permission))
			return false;
	return true;
}

bool ModExecutionContext::IsMapped(std::uint32_t address, std::size_t size) const
{
	std::size_t offset{};
	if (!ValidateRange(address, size, offset))
		return false;
	const auto firstPage = offset / kPageSize;
	const auto lastPage = (offset + size - 1) / kPageSize;
	for (auto page = firstPage; page <= lastPage; ++page)
		if (m_pages[page] == ModMemoryPermission::None)
			return false;
	return true;
}

bool ModExecutionContext::Map(std::uint32_t address, std::span<const std::byte> initialData,
	std::uint32_t mappedSize, ModMemoryPermission permissions)
{
	std::size_t offset{};
	const auto flags = static_cast<std::uint8_t>(permissions);
	if (IsStopped() || address % kPageSize != 0 || mappedSize == 0 || mappedSize % kPageSize != 0 ||
		initialData.size() > mappedSize || !ValidateRange(address, mappedSize, offset) || flags == 0 ||
		((flags & static_cast<std::uint8_t>(ModMemoryPermission::Write)) &&
		 (flags & static_cast<std::uint8_t>(ModMemoryPermission::Execute))))
	{
		Stop(ModFaultReason::InvalidMapping, address, permissions);
		return false;
	}
	const auto firstPage = offset / kPageSize;
	const auto pageCount = mappedSize / kPageSize;
	if (!std::all_of(m_pages.begin() + firstPage, m_pages.begin() + firstPage + pageCount,
		[](ModMemoryPermission page) { return page == ModMemoryPermission::None; }))
	{
		Stop(ModFaultReason::InvalidMapping, address, permissions);
		return false;
	}
	std::fill(m_pages.begin() + firstPage, m_pages.begin() + firstPage + pageCount, permissions);
	if (!initialData.empty())
		std::memcpy(m_memory.data() + offset, initialData.data(), initialData.size());
	return true;
}

std::byte* ModExecutionContext::Resolve(std::uint32_t address, std::size_t size,
	ModMemoryPermission permission)
{
	std::size_t offset{};
	if (IsStopped())
		return nullptr;
	if (!ValidateRange(address, size, offset))
	{
		Stop(ModFaultReason::Unmapped, address, permission);
		return nullptr;
	}
	if (!IsMapped(address, size))
	{
		Stop(ModFaultReason::Unmapped, address, permission);
		return nullptr;
	}
	if (!HasPermission(address, size, permission))
	{
		Stop(ModFaultReason::PermissionDenied, address, permission);
		return nullptr;
	}
	return m_memory.data() + offset;
}

const std::byte* ModExecutionContext::Resolve(std::uint32_t address, std::size_t size,
	ModMemoryPermission permission) const
{
	if (IsStopped() || !HasPermission(address, size, permission))
		return nullptr;
	return m_memory.data() + (address - m_virtualBase);
}

void ModExecutionContext::AllowHle(std::uint16_t hleId)
{
	if (!IsStopped())
		m_allowedHles.insert(hleId);
}

bool ModExecutionContext::IsHleAllowed(std::uint16_t hleId) const
{
	return !IsStopped() && m_allowedHles.contains(hleId);
}

void ModExecutionContext::BeginFrame()
{
	if (m_frameQuotaExceeded)
		m_consecutiveQuotaFrames = std::min<std::uint8_t>(m_consecutiveQuotaFrames + 1, 3);
	else
		m_consecutiveQuotaFrames = 0;
	m_frameQuotaExceeded = false;
	m_frameInstructions = 0;
	m_frameStart = std::chrono::steady_clock::now();
}

bool ModExecutionContext::ConsumeInstructions(std::uint64_t count)
{
	if (IsStopped() || count > kMaximumInstructionsPerFrame -
		std::min(m_frameInstructions, kMaximumInstructionsPerFrame))
	{
		MarkQuotaExceeded(ModFaultReason::InstructionQuota);
		return false;
	}
	m_frameInstructions += count;
	return true;
}

bool ModExecutionContext::CheckWallClock(std::chrono::steady_clock::time_point now,
	std::chrono::nanoseconds budget)
{
	if (IsStopped() || now - m_frameStart > budget)
	{
		MarkQuotaExceeded(ModFaultReason::WallClockQuota);
		return false;
	}
	return true;
}

void ModExecutionContext::MarkQuotaExceeded(ModFaultReason reason)
{
	if (reason != ModFaultReason::InstructionQuota && reason != ModFaultReason::WallClockQuota)
		return;
	m_frameQuotaExceeded = true;
	if (m_consecutiveQuotaFrames >= 2)
		Stop(reason);
}

void ModExecutionContext::Stop(ModFaultReason reason, std::uint32_t address,
	ModMemoryPermission access)
{
	bool expected = false;
	if (m_stopped.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		m_fault = {reason, address, access};
}
