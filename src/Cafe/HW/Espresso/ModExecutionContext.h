#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

enum class ModMemoryPermission : std::uint8_t
{
	None = 0,
	Read = 1U << 0,
	Write = 1U << 1,
	Execute = 1U << 2,
};

constexpr ModMemoryPermission operator|(ModMemoryPermission left, ModMemoryPermission right)
{
	return static_cast<ModMemoryPermission>(static_cast<std::uint8_t>(left) |
		static_cast<std::uint8_t>(right));
}

enum class ModFaultReason : std::uint8_t
{
	None,
	Unmapped,
	PermissionDenied,
	InvalidMapping,
	DisallowedHle,
	InstructionQuota,
	WallClockQuota,
};

struct ModFault
{
	ModFaultReason reason{};
	std::uint32_t address{};
	ModMemoryPermission access{};
};

// Host-owned execution state for one isolated PPC Mod. The title CPU always has
// a null context and retains its existing fast path.
class ModExecutionContext
{
public:
	static constexpr std::uint32_t kPageSize = 4096;
	static constexpr std::uint32_t kMaximumCodeBytes = 16U * 1024U * 1024U;
	static constexpr std::uint32_t kMaximumPrivateBytes = 32U * 1024U * 1024U;
	static constexpr std::uint32_t kMaximumStackBytes = 1U * 1024U * 1024U;
	static constexpr std::uint32_t kMaximumAddressSpaceBytes =
		kMaximumCodeBytes + kMaximumPrivateBytes + kMaximumStackBytes;
	static constexpr std::uint64_t kMaximumInstructionsPerFrame = 1'000'000;

	ModExecutionContext(std::uint64_t addressSpaceId, std::uint32_t generation,
		std::string principal, std::uint32_t virtualBase = 0x10000000,
		std::uint32_t size = kMaximumAddressSpaceBytes);

	[[nodiscard]] bool Map(std::uint32_t address, std::span<const std::byte> initialData,
		std::uint32_t mappedSize, ModMemoryPermission permissions);
	[[nodiscard]] std::byte* Resolve(std::uint32_t address, std::size_t size,
		ModMemoryPermission permission);
	[[nodiscard]] const std::byte* Resolve(std::uint32_t address, std::size_t size,
		ModMemoryPermission permission) const;

	void AllowHle(std::uint16_t hleId);
	[[nodiscard]] bool IsHleAllowed(std::uint16_t hleId) const;
	void BeginFrame();
	[[nodiscard]] bool ConsumeInstructions(std::uint64_t count = 1);
	[[nodiscard]] bool CheckWallClock(std::chrono::steady_clock::time_point now,
		std::chrono::nanoseconds budget = std::chrono::milliseconds(1));
	void MarkQuotaExceeded(ModFaultReason reason);
	void Stop(ModFaultReason reason, std::uint32_t address = 0,
		ModMemoryPermission access = ModMemoryPermission::None);

	[[nodiscard]] bool IsStopped() const { return m_stopped.load(std::memory_order_acquire); }
	[[nodiscard]] ModFault Fault() const { return m_fault; }
	[[nodiscard]] std::uint64_t AddressSpaceId() const { return m_addressSpaceId; }
	[[nodiscard]] std::uint32_t Generation() const { return m_generation; }
	[[nodiscard]] const std::string& Principal() const { return m_principal; }
	[[nodiscard]] std::uint32_t VirtualBase() const { return m_virtualBase; }
	[[nodiscard]] std::uint32_t AddressSpaceSize() const { return static_cast<std::uint32_t>(m_memory.size()); }
	void SetGrantedPermissions(std::uint32_t permissions) { m_grantedPermissions = permissions; }
	[[nodiscard]] std::uint32_t GrantedPermissions() const { return m_grantedPermissions; }
	void SetTitleId(std::uint64_t titleId) { m_titleId = titleId; }
	[[nodiscard]] std::uint64_t TitleId() const { return m_titleId; }

private:
	[[nodiscard]] bool ValidateRange(std::uint32_t address, std::size_t size,
		std::size_t& offset) const;
	[[nodiscard]] bool HasPermission(std::uint32_t address, std::size_t size,
		ModMemoryPermission permission) const;
	[[nodiscard]] bool IsMapped(std::uint32_t address, std::size_t size) const;

	std::uint64_t m_addressSpaceId{};
	std::uint32_t m_generation{};
	std::string m_principal;
	std::uint32_t m_grantedPermissions{};
	std::uint64_t m_titleId{};
	std::uint32_t m_virtualBase{};
	std::vector<std::byte> m_memory;
	std::vector<ModMemoryPermission> m_pages;
	std::unordered_set<std::uint16_t> m_allowedHles;
	std::atomic_bool m_stopped{};
	ModFault m_fault{};
	std::uint64_t m_frameInstructions{};
	std::chrono::steady_clock::time_point m_frameStart{};
	std::uint8_t m_consecutiveQuotaFrames{};
	bool m_frameQuotaExceeded{};
};
