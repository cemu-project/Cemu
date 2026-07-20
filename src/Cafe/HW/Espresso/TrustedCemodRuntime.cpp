#include "Common/precompiled.h"

#include "Cafe/HW/Espresso/TrustedCemodRuntime.h"

#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "Cafe/OS/libs/cemuextend/Cex2Host.h"
#include "Cafe/OS/libs/cemuextend/Cex2Owner.h"

#include <atomic>
#include <map>
#include <mutex>
#include <set>

namespace
{
	constexpr std::uint32_t kBootstrapMagic = 0x434d4231U; // CMB1
	constexpr std::uint16_t kBootstrapVersion = 1;
	constexpr std::uint16_t kBootstrapRecordSize = 24;
	constexpr std::uint32_t kMaximumImageSize = 8U * 1024U * 1024U;

	std::uint16_t U16(std::span<const std::byte> bytes, std::size_t offset)
	{
		return (std::to_integer<std::uint16_t>(bytes[offset]) << 8) |
			std::to_integer<std::uint16_t>(bytes[offset + 1]);
	}

	std::uint32_t U32(std::span<const std::byte> bytes, std::size_t offset)
	{
		return (std::to_integer<std::uint32_t>(bytes[offset]) << 24) |
			(std::to_integer<std::uint32_t>(bytes[offset + 1]) << 16) |
			(std::to_integer<std::uint32_t>(bytes[offset + 2]) << 8) |
			std::to_integer<std::uint32_t>(bytes[offset + 3]);
	}

	std::uint16_t Read16(std::uint32_t address)
	{
		const auto* value = static_cast<const std::uint8_t*>(memory_getPointerFromVirtualOffset(address));
		return static_cast<std::uint16_t>((value[0] << 8) | value[1]);
	}

	std::uint32_t Read32(std::uint32_t address)
	{
		const auto* value = static_cast<const std::uint8_t*>(memory_getPointerFromVirtualOffset(address));
		return (static_cast<std::uint32_t>(value[0]) << 24) |
			(static_cast<std::uint32_t>(value[1]) << 16) |
			(static_cast<std::uint32_t>(value[2]) << 8) | value[3];
	}

	void Write16(std::uint32_t address, std::uint16_t value)
	{
		auto* output = static_cast<std::uint8_t*>(memory_getPointerFromVirtualOffset(address));
		output[0] = static_cast<std::uint8_t>(value >> 8);
		output[1] = static_cast<std::uint8_t>(value);
	}

	void Write32(std::uint32_t address, std::uint32_t value)
	{
		auto* output = static_cast<std::uint8_t*>(memory_getPointerFromVirtualOffset(address));
		output[0] = static_cast<std::uint8_t>(value >> 24);
		output[1] = static_cast<std::uint8_t>(value >> 16);
		output[2] = static_cast<std::uint8_t>(value >> 8);
		output[3] = static_cast<std::uint8_t>(value);
	}

	bool AddU32(std::uint32_t left, std::uint32_t right, std::uint32_t& output)
	{
		const auto value = static_cast<std::uint64_t>(left) + right;
		if (value > std::numeric_limits<std::uint32_t>::max()) return false;
		output = static_cast<std::uint32_t>(value);
		return true;
	}

	struct LoadSegment
	{
		std::uint32_t virtualAddress{};
		std::uint32_t fileOffset{};
		std::uint32_t fileSize{};
		std::uint32_t memorySize{};
		std::uint32_t flags{};
	};

	struct Section
	{
		std::uint32_t name{};
		std::uint32_t type{};
		std::uint32_t flags{};
		std::uint32_t address{};
		std::uint32_t offset{};
		std::uint32_t size{};
		std::uint32_t link{};
		std::uint32_t info{};
		std::uint32_t alignment{};
		std::uint32_t entrySize{};
	};

	struct ParsedElf
	{
		std::vector<LoadSegment> segments;
		std::vector<Section> sections;
		std::uint32_t minimumAddress{};
		std::uint32_t imageSize{};
		std::size_t bootstrapSection{};
	};

	bool ParseElf(const CemodPackage& package, ParsedElf& parsed, std::string& error)
	{
		const std::span<const std::byte> elf(package.elf);
		if (elf.size() < 52 || U16(elf, 16) != 3 || U16(elf, 18) != 20)
		{
			error = "trusted native executable must be an ET_DYN PPC ELF";
			return false;
		}
		const auto programOffset = U32(elf, 28);
		const auto sectionOffset = U32(elf, 32);
		const auto programSize = U16(elf, 42);
		const auto programCount = U16(elf, 44);
		const auto sectionSize = U16(elf, 46);
		const auto sectionCount = U16(elf, 48);
		const auto nameSection = U16(elf, 50);
		if (programSize < 32 || programCount == 0 || programCount > 128 || sectionSize < 40 ||
			sectionCount == 0 || sectionCount > 1024 || nameSection >= sectionCount ||
			programOffset > elf.size() || static_cast<std::uint64_t>(programCount) * programSize > elf.size() - programOffset ||
			sectionOffset > elf.size() || static_cast<std::uint64_t>(sectionCount) * sectionSize > elf.size() - sectionOffset)
		{
			error = "trusted ELF tables are out of bounds";
			return false;
		}
		std::uint32_t minimum = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t maximum{};
		for (std::uint16_t index = 0; index < programCount; ++index)
		{
			const auto offset = programOffset + static_cast<std::uint32_t>(index) * programSize;
			if (U32(elf, offset) != 1) continue;
			LoadSegment segment{U32(elf, offset + 8), U32(elf, offset + 4), U32(elf, offset + 16),
				U32(elf, offset + 20), U32(elf, offset + 24)};
			std::uint32_t end{};
			if (segment.memorySize == 0 || segment.fileSize > segment.memorySize ||
				segment.fileOffset > elf.size() || segment.fileSize > elf.size() - segment.fileOffset ||
				((segment.flags & 3U) == 3U) || !AddU32(segment.virtualAddress, segment.memorySize, end))
			{
				error = "trusted ELF contains an invalid or writable-executable segment";
				return false;
			}
			for (const auto& existing : parsed.segments)
			{
				const auto existingEnd = existing.virtualAddress + existing.memorySize;
				if (segment.virtualAddress < existingEnd && existing.virtualAddress < end)
				{
					error = "trusted ELF load segments overlap";
					return false;
				}
			}
			parsed.segments.push_back(segment);
			minimum = std::min(minimum, segment.virtualAddress);
			maximum = std::max(maximum, end);
		}
		if (parsed.segments.empty() || maximum <= minimum || maximum - minimum > kMaximumImageSize)
		{
			error = "trusted ELF image exceeds the shared codecave";
			return false;
		}
		parsed.minimumAddress = minimum;
		parsed.imageSize = (maximum - minimum + 255U) & ~255U;
		parsed.sections.reserve(sectionCount);
		for (std::uint16_t index = 0; index < sectionCount; ++index)
		{
			const auto offset = sectionOffset + static_cast<std::uint32_t>(index) * sectionSize;
			parsed.sections.push_back({U32(elf, offset), U32(elf, offset + 4), U32(elf, offset + 8),
				U32(elf, offset + 12), U32(elf, offset + 16), U32(elf, offset + 20),
				U32(elf, offset + 24), U32(elf, offset + 28), U32(elf, offset + 32), U32(elf, offset + 36)});
		}
		const auto& strings = parsed.sections[nameSection];
		if (strings.offset > elf.size() || strings.size > elf.size() - strings.offset)
		{
			error = "trusted ELF section-name table is invalid";
			return false;
		}
		parsed.bootstrapSection = parsed.sections.size();
		for (std::size_t index = 0; index < parsed.sections.size(); ++index)
		{
			const auto& section = parsed.sections[index];
			if (section.name >= strings.size) continue;
			const char* name = reinterpret_cast<const char*>(elf.data() + strings.offset + section.name);
			const auto maximumName = strings.size - section.name;
			const auto length = strnlen(name, maximumName);
			if (length == maximumName) continue;
			if (std::string_view(name, length) == ".cemod.bootstrap")
			{
				if (parsed.bootstrapSection != parsed.sections.size())
				{
					error = "trusted ELF contains duplicate bootstrap sections";
					return false;
				}
				parsed.bootstrapSection = index;
			}
		}
		if (parsed.bootstrapSection == parsed.sections.size())
		{
			error = "trusted ELF is missing .cemod.bootstrap";
			return false;
		}
		const auto& bootstrap = parsed.sections[parsed.bootstrapSection];
		if ((bootstrap.flags & 2U) == 0 || bootstrap.address < minimum ||
			bootstrap.size < 12 || bootstrap.size > maximum - bootstrap.address)
		{
			error = "trusted ELF bootstrap section is not loadable";
			return false;
		}
		return true;
	}

	bool ContainsImageRange(const ParsedElf& elf, std::uint32_t virtualAddress, std::uint32_t size)
	{
		if (size == 0) return false;
		for (const auto& segment : elf.segments)
			if (virtualAddress >= segment.virtualAddress &&
				virtualAddress - segment.virtualAddress <= segment.memorySize &&
				size <= segment.memorySize - (virtualAddress - segment.virtualAddress)) return true;
		return false;
	}

	bool IsExecutableAddress(const ParsedElf& elf, std::uint32_t virtualAddress)
	{
		for (const auto& segment : elf.segments)
			if ((segment.flags & 1U) && virtualAddress >= segment.virtualAddress &&
				virtualAddress - segment.virtualAddress < segment.memorySize) return true;
		return false;
	}

	bool ApplyRelocations(const CemodPackage& package, const ParsedElf& parsed,
		std::uint32_t loadBias, std::string& error)
	{
		const std::span<const std::byte> elf(package.elf);
		for (const auto& relocationSection : parsed.sections)
		{
			if (relocationSection.type != 4) continue;
			if (relocationSection.entrySize < 12 || relocationSection.size % relocationSection.entrySize != 0 ||
				relocationSection.offset > elf.size() || relocationSection.size > elf.size() - relocationSection.offset ||
				relocationSection.link >= parsed.sections.size())
			{
				error = "trusted ELF relocation table is invalid";
				return false;
			}
			const auto& symbols = parsed.sections[relocationSection.link];
			if ((symbols.type != 2 && symbols.type != 11) || symbols.entrySize < 16 ||
				symbols.size % symbols.entrySize != 0 || symbols.offset > elf.size() || symbols.size > elf.size() - symbols.offset)
			{
				error = "trusted ELF relocation symbol table is invalid";
				return false;
			}
			for (std::uint32_t index = 0; index < relocationSection.size / relocationSection.entrySize; ++index)
			{
				const auto offset = relocationSection.offset + index * relocationSection.entrySize;
				const auto targetVirtual = U32(elf, offset);
				const auto info = U32(elf, offset + 4);
				const auto addend = static_cast<std::int32_t>(U32(elf, offset + 8));
				const auto type = static_cast<std::uint8_t>(info);
				const auto symbolIndex = info >> 8;
				if (symbolIndex >= symbols.size / symbols.entrySize || !ContainsImageRange(parsed, targetVirtual,
					(type == 4 || type == 5 || type == 6) ? 2U : 4U))
				{
					error = "trusted ELF relocation target is out of range";
					return false;
				}
				std::uint64_t symbolValue{};
				if (symbolIndex != 0)
				{
					const auto symbolOffset = symbols.offset + symbolIndex * symbols.entrySize;
					const auto sectionIndex = U16(elf, symbolOffset + 14);
					if (sectionIndex == 0)
					{
						error = "trusted ELF contains an undefined relocation symbol";
						return false;
					}
					const auto value = U32(elf, symbolOffset + 4);
					symbolValue = sectionIndex == 0xfff1U ? value : static_cast<std::uint64_t>(loadBias) + value;
				}
				const auto place = static_cast<std::uint64_t>(loadBias) + targetVirtual;
				const auto absoluteSigned = static_cast<std::int64_t>(symbolValue) + addend;
				const auto relativeSigned = absoluteSigned - static_cast<std::int64_t>(place);
				const auto target = static_cast<std::uint32_t>(place);
				switch (type)
				{
				case 0: break; // R_PPC_NONE: padding/no-op relocation
				case 1:
					if (absoluteSigned < 0 || absoluteSigned > std::numeric_limits<std::uint32_t>::max())
						{ error = "R_PPC_ADDR32 overflow"; return false; }
					Write32(target, static_cast<std::uint32_t>(absoluteSigned)); break;
				case 4: Write16(target, static_cast<std::uint16_t>(absoluteSigned)); break;
				case 5: Write16(target, static_cast<std::uint16_t>(static_cast<std::uint64_t>(absoluteSigned) >> 16)); break;
				case 6: Write16(target, static_cast<std::uint16_t>((static_cast<std::uint64_t>(absoluteSigned) + 0x8000U) >> 16)); break;
				case 10:
					if ((relativeSigned & 3) != 0 || relativeSigned < -0x02000000LL || relativeSigned > 0x01fffffcLL)
						{ error = "R_PPC_REL24 overflow"; return false; }
					Write32(target, (Read32(target) & 0xfc000003U) |
						(static_cast<std::uint32_t>(relativeSigned) & 0x03fffffcU)); break;
				case 22:
				{
					const auto value = static_cast<std::int64_t>(loadBias) + addend;
					if (value < 0 || value > std::numeric_limits<std::uint32_t>::max())
						{ error = "R_PPC_RELATIVE overflow"; return false; }
					Write32(target, static_cast<std::uint32_t>(value)); break;
				}
				case 26:
					if (relativeSigned < std::numeric_limits<std::int32_t>::min() ||
						relativeSigned > std::numeric_limits<std::int32_t>::max())
						{ error = "R_PPC_REL32 overflow"; return false; }
					Write32(target, static_cast<std::uint32_t>(relativeSigned)); break;
				default: error = fmt::format("unsupported trusted ELF relocation type {}", type); return false;
				}
			}
		}
		return true;
	}

	std::optional<std::uint32_t> ResolveModuleAddress(const RPLModule& module,
		std::uint32_t virtualAddress)
	{
		for (std::uint16_t index = 0; index < module.rplHeader.sectionTableEntryCount; ++index)
		{
			const auto& section = module.sectionTablePtr[index];
			const auto start = static_cast<std::uint32_t>(section.virtualAddress);
			const auto size = static_cast<std::uint32_t>(section.sectionSize);
			if (virtualAddress >= start && virtualAddress - start < size &&
				index < module.sectionAddressTable2.size() && module.sectionAddressTable2[index].ptr)
			{
				return memory_getVirtualOffsetFromPointer(module.sectionAddressTable2[index].ptr) +
					virtualAddress - start;
			}
		}
		return std::nullopt;
	}

	class TrustedTitleOwner final : public cemuextend_hle::Cex2Owner
	{
	public:
		TrustedTitleOwner(std::uint64_t titleId, std::uint32_t generation,
			std::uint32_t permissions, ModServicePermissions services)
			: m_addressSpaceId(0x8000000000000000ULL | (titleId & 0x7fffffffffffffffULL)),
			  m_generation(generation), m_principal(fmt::format("trusted-title:{:016x}", titleId)),
			  m_titleId(titleId), m_permissions(permissions)
		{
			SetServices(services);
		}

		std::uint64_t AddressSpaceId() const override { return m_addressSpaceId; }
		std::uint32_t Generation() const override { return m_generation; }
		const std::string& Principal() const override { return m_principal; }
		std::uint64_t TitleId() const override { return m_titleId; }
		bool IsStopped() const override { return m_stopped.load(std::memory_order_acquire); }
		std::uint32_t GrantedPermissions() const override { return m_permissions.load(std::memory_order_acquire); }
		void SetGrantedPermissions(std::uint32_t value) override { m_permissions.store(value, std::memory_order_release); }
		void Stop() { m_stopped.store(true, std::memory_order_release); }
		void SetServices(ModServicePermissions services)
		{
			m_read.store(services.readMask, std::memory_order_release);
			m_write.store(services.writeMask, std::memory_order_release);
			m_inject.store(services.injectMask, std::memory_order_release);
		}
		bool IsServiceAllowed(std::uint16_t service, std::uint32_t permission,
			std::uint16_t operation) const override
		{
			if (service == 1 || permission == 0) return true;
			if (service < 2 || service > 9) return false;
			const auto bit = 1U << (service - 1U);
			if (permission == 1) return (m_read.load(std::memory_order_acquire) & bit) != 0;
			if (permission == 2) return (m_write.load(std::memory_order_acquire) & bit) != 0;
			if (permission == 4) return (m_inject.load(std::memory_order_acquire) & bit) != 0;
			if (permission == 8) return (((operation == 1 ? m_read : m_write).load(std::memory_order_acquire)) & bit) != 0;
			if (permission == 16) return (m_read.load(std::memory_order_acquire) & bit) != 0;
			return false;
		}

	private:
		std::uint64_t m_addressSpaceId{};
		std::uint32_t m_generation{};
		std::string m_principal;
		std::uint64_t m_titleId{};
		std::atomic<std::uint32_t> m_permissions{};
		std::atomic<std::uint32_t> m_read{};
		std::atomic<std::uint32_t> m_write{};
		std::atomic<std::uint32_t> m_inject{};
		std::atomic_bool m_stopped{};
	};
}

struct TrustedCemodRuntime::Impl
{
	struct Instance
	{
		CemodPackage package;
		MEMPTR<void> allocation;
		std::uint32_t allocationSize{};
		std::uint32_t patchAddress{};
		std::uint32_t originalInstruction{};
	};

	mutable std::mutex mutex;
	std::map<std::uint64_t, Instance> mods;
	std::set<std::uint32_t> patchAddresses;
	std::unique_ptr<TrustedTitleOwner> owner;
	std::uint64_t nextHandle{1};
	std::uint32_t nextGeneration{1};
};

TrustedCemodRuntime::TrustedCemodRuntime() : m_impl(std::make_unique<Impl>()) {}
TrustedCemodRuntime::~TrustedCemodRuntime() { UnloadAll(); }

std::optional<std::uint64_t> TrustedCemodRuntime::Load(CemodPackage package,
	std::uint32_t titlePermissions, const ModServicePermissions& services, std::string& error)
{
	std::lock_guard lock(m_impl->mutex);
	if (!package.IsTrustedNative()) { error = "package is not trusted_native"; return std::nullopt; }
	if (!m_impl->mods.empty() && m_impl->owner && m_impl->owner->TitleId() != package.targetTitleId)
		{ error = "trusted runtime already belongs to another title"; return std::nullopt; }
	for (const auto& [handle, mod] : m_impl->mods)
		if (mod.package.manifest.modId == package.manifest.modId)
			{ error = "duplicate trusted native mod_id"; return std::nullopt; }

	ParsedElf parsed;
	if (!ParseElf(package, parsed, error)) return std::nullopt;
	const auto allocation = RPLLoader_AllocateCodeCaveMem(256, parsed.imageSize);
	if (!allocation) { error = "shared codecave does not have enough space"; return std::nullopt; }
	auto releaseOnFailure = true;
	auto release = [&] { if (releaseOnFailure) RPLLoader_ReleaseCodeCaveMem(allocation); };
	const auto allocationAddress = allocation.GetMPTR();
	if (allocationAddress < parsed.minimumAddress)
		{ error = "trusted ELF load bias underflow"; release(); return std::nullopt; }
	const auto loadBias = allocationAddress - parsed.minimumAddress;
	std::memset(allocation.GetPtr(), 0, parsed.imageSize);
	for (const auto& segment : parsed.segments)
	{
		const auto destination = loadBias + segment.virtualAddress;
		std::memcpy(memory_getPointerFromVirtualOffset(destination),
			package.elf.data() + segment.fileOffset, segment.fileSize);
	}
	if (!ApplyRelocations(package, parsed, loadBias, error))
		{ release(); return std::nullopt; }

	const auto& bootstrapSection = parsed.sections[parsed.bootstrapSection];
	const auto bootstrapAddress = loadBias + bootstrapSection.address;
	if (Read32(bootstrapAddress) != kBootstrapMagic || Read16(bootstrapAddress + 4) != kBootstrapVersion ||
		Read16(bootstrapAddress + 6) != kBootstrapRecordSize)
		{ error = "invalid CMB1 bootstrap header"; release(); return std::nullopt; }
	const auto recordCount = Read32(bootstrapAddress + 8);
	if (recordCount == 0 || recordCount > 64 || bootstrapSection.size != 12U + recordCount * kBootstrapRecordSize)
		{ error = "invalid CMB1 bootstrap record count"; release(); return std::nullopt; }

	std::optional<std::pair<const RPLModule*, std::uint32_t>> match;
	std::uint32_t handlerVirtual{};
	std::uint32_t expected{};
	std::uint32_t mask{};
	for (std::uint32_t record = 0; record < recordCount; ++record)
	{
		const auto address = bootstrapAddress + 12 + record * kBootstrapRecordSize;
		const auto moduleHash = Read32(address);
		const auto targetVirtual = Read32(address + 4);
		const auto recordExpected = Read32(address + 8);
		const auto recordMask = Read32(address + 12);
		const auto recordHandler = Read32(address + 16);
		const auto flags = Read32(address + 20);
		if (flags != 0 || recordMask == 0 || recordHandler < loadBias ||
			!IsExecutableAddress(parsed, recordHandler - loadBias))
			{ error = "invalid CMB1 bootstrap record"; release(); return std::nullopt; }
		for (sint32 moduleIndex = 0; moduleIndex < RPLLoader_GetModuleCount(); ++moduleIndex)
		{
			const auto* module = RPLLoader_GetModuleList()[moduleIndex];
			if (!module || module->patchCRC != moduleHash) continue;
			const auto target = ResolveModuleAddress(*module, targetVirtual);
			if (!target) { error = "bootstrap target is outside the matching module"; release(); return std::nullopt; }
			if (match) { error = "bootstrap records match more than one loaded module"; release(); return std::nullopt; }
			match = std::pair{module, *target};
			handlerVirtual = recordHandler;
			expected = recordExpected;
			mask = recordMask;
		}
	}
	if (!match) { error = "no CMB1 bootstrap record matches a loaded module"; release(); return std::nullopt; }
	const auto patchAddress = match->second;
	if ((patchAddress & 3U) != 0 || !memory_isAddressRangeAccessible(patchAddress, 4) ||
		m_impl->patchAddresses.contains(patchAddress))
		{ error = "bootstrap patch target is invalid or already claimed"; release(); return std::nullopt; }
	const auto original = Read32(patchAddress);
	if ((original & mask) != (expected & mask))
		{ error = "bootstrap target instruction does not match the package"; release(); return std::nullopt; }
	const auto handlerAddress = handlerVirtual;
	const auto displacement = static_cast<std::int64_t>(handlerAddress) - patchAddress;
	if ((displacement & 3) != 0 || displacement < -0x02000000LL || displacement > 0x01fffffcLL)
		{ error = "bootstrap branch is outside the PPC REL24 range"; release(); return std::nullopt; }
	Write32(patchAddress, 0x48000000U | (static_cast<std::uint32_t>(displacement) & 0x03fffffcU));
	PPCRecompiler_invalidateRange(patchAddress, patchAddress + 4);
	m_impl->patchAddresses.insert(patchAddress);
	if (!m_impl->owner)
		m_impl->owner = std::make_unique<TrustedTitleOwner>(package.targetTitleId,
			m_impl->nextGeneration++, titlePermissions, services);
	else
	{
		m_impl->owner->SetGrantedPermissions(titlePermissions);
		m_impl->owner->SetServices(services);
	}
	const auto handle = m_impl->nextHandle++;
	releaseOnFailure = false;
	m_impl->mods.emplace(handle, Impl::Instance{std::move(package), allocation,
		parsed.imageSize, patchAddress, original});
	return handle;
}

bool TrustedCemodRuntime::Unload(std::uint64_t handle)
{
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->mods.find(handle);
	if (found == m_impl->mods.end()) return false;
	Write32(found->second.patchAddress, found->second.originalInstruction);
	PPCRecompiler_invalidateRange(found->second.patchAddress, found->second.patchAddress + 4);
	m_impl->patchAddresses.erase(found->second.patchAddress);
	RPLLoader_ReleaseCodeCaveMem(found->second.allocation);
	m_impl->mods.erase(found);
	if (m_impl->mods.empty() && m_impl->owner)
	{
		cemuextend_hle::Cex2Host::Instance().CloseOwner(*m_impl->owner);
		m_impl->owner->Stop();
		m_impl->owner.reset();
	}
	return true;
}

void TrustedCemodRuntime::UnloadAll()
{
	for (;;)
	{
		std::uint64_t handle{};
		{
			std::lock_guard lock(m_impl->mutex);
			if (m_impl->mods.empty()) break;
			handle = m_impl->mods.rbegin()->first;
		}
		(void)Unload(handle);
	}
}

void TrustedCemodRuntime::UpdatePermissions(std::uint32_t permissions,
	const ModServicePermissions& services)
{
	std::lock_guard lock(m_impl->mutex);
	if (!m_impl->owner) return;
	m_impl->owner->SetServices(services);
	cemuextend_hle::Cex2Host::Instance().PermissionsChanged(*m_impl->owner, permissions);
}

cemuextend_hle::Cex2Owner* TrustedCemodRuntime::Owner()
{
	std::lock_guard lock(m_impl->mutex);
	return m_impl->owner.get();
}

std::size_t TrustedCemodRuntime::Size() const
{
	std::lock_guard lock(m_impl->mutex);
	return m_impl->mods.size();
}
