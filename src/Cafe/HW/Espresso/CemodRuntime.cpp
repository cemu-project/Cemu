#include "Common/precompiled.h"

#include "Cafe/HW/Espresso/CemodRuntime.h"

#include "Cafe/HW/Espresso/ModExecutionContext.h"
#include "Cafe/HW/Espresso/TrustedCemodRuntime.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/Recompiler/PPCRecompiler.h"
#include "Cafe/OS/libs/cemuextend/Cex2Host.h"
#include "Cafe/OS/libs/cemuextend/cemuextend.h"
#include "Cafe/OS/common/OSCommon.h"

#include <chrono>
#include <map>
#include <mutex>
#include <set>

namespace {

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

std::uint32_t AlignDown(std::uint32_t value)
{
	return value & ~(ModExecutionContext::kPageSize - 1);
}

bool AlignUp(std::uint64_t value, std::uint32_t& output)
{
	value = (value + ModExecutionContext::kPageSize - 1) &
		~static_cast<std::uint64_t>(ModExecutionContext::kPageSize - 1);
	if (value > std::numeric_limits<std::uint32_t>::max())
		return false;
	output = static_cast<std::uint32_t>(value);
	return true;
}

struct Segment
{
	std::uint32_t address{};
	std::uint32_t fileOffset{};
	std::uint32_t fileSize{};
	std::uint32_t mappedSize{};
	ModMemoryPermission permissions{};
};

struct Entrypoints
{
	std::uint32_t init{};
	std::uint32_t tick{};
	std::uint32_t event{};
	std::uint32_t shutdown{};
};

struct HleImport
{
	std::string name;
	std::uint32_t address{};
};

bool ParseElf(const CemodPackage& package, std::vector<Segment>& segments,
	Entrypoints& entrypoints, std::vector<HleImport>& imports,
	std::uint32_t& virtualBase, std::uint32_t& stackBase,
	std::uint32_t& addressSpaceSize, std::string& error)
{
	const std::span<const std::byte> elf(package.elf);
	if (elf.size() < 52)
		return false;
	const auto programOffset = U32(elf, 28);
	const auto sectionOffset = U32(elf, 32);
	const auto programSize = U16(elf, 42);
	const auto programCount = U16(elf, 44);
	const auto sectionSize = U16(elf, 46);
	const auto sectionCount = U16(elf, 48);
	if (programSize < 32 || programCount == 0 || programCount > 128 ||
		programOffset > elf.size() ||
		static_cast<std::uint64_t>(programCount) * programSize > elf.size() - programOffset)
	{
		error = "PPC ELF program table is out of bounds";
		return false;
	}
	std::uint32_t lowest = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t highest{};
	std::uint64_t codeBytes{};
	std::uint64_t privateBytes{};
	for (std::uint16_t index = 0; index < programCount; ++index)
	{
		const auto offset = programOffset + static_cast<std::uint32_t>(index) * programSize;
		if (U32(elf, offset) != 1)
			continue;
		const auto fileOffset = U32(elf, offset + 4);
		const auto address = U32(elf, offset + 8);
		const auto fileSize = U32(elf, offset + 16);
		const auto memorySize = U32(elf, offset + 20);
		const auto flags = U32(elf, offset + 24);
		std::uint32_t mappedSize{};
		if (address % ModExecutionContext::kPageSize != 0 ||
			!AlignUp(memorySize, mappedSize) || mappedSize == 0 ||
			fileSize > memorySize || fileOffset > elf.size() || fileSize > elf.size() - fileOffset ||
			((flags & 1U) != 0 && (flags & 2U) != 0) ||
			address > std::numeric_limits<std::uint32_t>::max() - mappedSize)
		{
			error = "PPC ELF load segments must be page-aligned and in range";
			return false;
		}
		ModMemoryPermission permission = ModMemoryPermission::Read;
		if ((flags & 1U) != 0) permission = permission | ModMemoryPermission::Execute;
		if ((flags & 2U) != 0) permission = permission | ModMemoryPermission::Write;
		segments.push_back({address, fileOffset, fileSize, mappedSize, permission});
		if ((flags & 1U) != 0)
			codeBytes += mappedSize;
		else
			privateBytes += mappedSize;
		lowest = std::min(lowest, address);
		highest = std::max(highest, address + mappedSize);
	}
	const auto declaredCodeBytes = (static_cast<std::uint64_t>(package.manifest.codeBytes) +
		ModExecutionContext::kPageSize - 1) & ~(static_cast<std::uint64_t>(ModExecutionContext::kPageSize) - 1);
	const auto declaredPrivateBytes = (static_cast<std::uint64_t>(package.manifest.privateBytes) +
		ModExecutionContext::kPageSize - 1) & ~(static_cast<std::uint64_t>(ModExecutionContext::kPageSize) - 1);
	if (codeBytes == 0 || codeBytes > declaredCodeBytes || privateBytes > declaredPrivateBytes)
	{
		error = "PPC ELF mappings exceed the manifest memory limits";
		return false;
	}
	if (segments.empty() || sectionCount == 0 || sectionSize < 40 ||
		sectionOffset > elf.size() || static_cast<std::uint64_t>(sectionCount) * sectionSize > elf.size() - sectionOffset)
	{
		error = "PPC ELF has no loadable segments or symbol table";
		return false;
	}

	std::map<std::string_view, std::uint32_t*> wanted{
		{"cemod_init", &entrypoints.init}, {"cemod_tick", &entrypoints.tick},
		{"cemod_event", &entrypoints.event}, {"cemod_shutdown", &entrypoints.shutdown},
	};
	const std::set<std::string_view> hleNames{
		"CEX2Query", "CEX2Open", "CEX2Submit", "CEX2Poll", "CEX2Cancel", "CEX2Close"};
	std::set<std::string> foundEntrypoints;
	std::set<std::string> foundImports;
	for (std::uint16_t section = 0; section < sectionCount; ++section)
	{
		const auto offset = sectionOffset + static_cast<std::uint32_t>(section) * sectionSize;
		const auto type = U32(elf, offset + 4);
		if (type != 2 && type != 11)
			continue;
		const auto symbolsOffset = U32(elf, offset + 16);
		const auto symbolsSize = U32(elf, offset + 20);
		const auto stringsIndex = U32(elf, offset + 24);
		const auto entrySize = U32(elf, offset + 36);
		if (entrySize < 16 || stringsIndex >= sectionCount || symbolsOffset > elf.size() ||
			symbolsSize > elf.size() - symbolsOffset || symbolsSize % entrySize != 0)
			continue;
		const auto stringsHeader = sectionOffset + stringsIndex * sectionSize;
		const auto stringsOffset = U32(elf, stringsHeader + 16);
		const auto stringsSize = U32(elf, stringsHeader + 20);
		if (stringsOffset > elf.size() || stringsSize > elf.size() - stringsOffset)
			continue;
		for (std::uint32_t symbol = 0; symbol < symbolsSize / entrySize; ++symbol)
		{
			const auto symbolOffset = symbolsOffset + symbol * entrySize;
			const auto nameOffset = U32(elf, symbolOffset);
			if (nameOffset >= stringsSize || std::to_integer<unsigned char>(elf[stringsOffset + nameOffset]) == 0)
				continue;
			const char* name = reinterpret_cast<const char*>(elf.data() + stringsOffset + nameOffset);
			const auto maximum = stringsSize - nameOffset;
			const auto length = strnlen(name, maximum);
			if (length == maximum)
				continue;
			const std::string_view symbolName(name, length);
			const auto symbolAddress = U32(elf, symbolOffset + 4);
			if (const auto found = wanted.find(symbolName); found != wanted.end())
			{
				if (!foundEntrypoints.emplace(symbolName).second)
				{ error = "PPC ELF contains a duplicate lifecycle entrypoint"; return false; }
				*found->second = symbolAddress;
			}
			if (hleNames.contains(symbolName) && symbolAddress != 0)
			{
				if (!foundImports.emplace(symbolName).second)
				{ error = "PPC ELF contains a duplicate CEX2 import stub"; return false; }
				imports.push_back({std::string(symbolName), symbolAddress});
			}
		}
	}
	if (!entrypoints.init || !entrypoints.tick || !entrypoints.event || !entrypoints.shutdown)
	{
		error = "PPC ELF must export cemod_init, cemod_tick, cemod_event, and cemod_shutdown";
		return false;
	}
	for (const auto address : {entrypoints.init, entrypoints.tick, entrypoints.event, entrypoints.shutdown})
	{
		const auto executable = std::ranges::any_of(segments, [address](const Segment& segment) {
			return address >= segment.address && address < segment.address + segment.mappedSize &&
				(static_cast<std::uint8_t>(segment.permissions) & static_cast<std::uint8_t>(ModMemoryPermission::Execute));
		});
		if (!executable)
		{
			error = "a lifecycle entrypoint is outside executable memory";
			return false;
		}
	}
	virtualBase = AlignDown(lowest);
	if (!AlignUp(highest, stackBase) || stackBase > std::numeric_limits<std::uint32_t>::max() - package.manifest.stackBytes ||
		!AlignUp(static_cast<std::uint64_t>(stackBase) + package.manifest.stackBytes - virtualBase, addressSpaceSize) ||
		addressSpaceSize > ModExecutionContext::kMaximumAddressSpaceBytes)
	{
		error = "PPC ELF address space plus stack exceeds the sandbox limit";
		return false;
	}
	return true;
}

bool BindHleImports(CemodPackage& package, const std::vector<Segment>& segments,
	const std::vector<HleImport>& imports, std::string& error)
{
	for (const auto& import : imports)
	{
		const auto segment = std::ranges::find_if(segments, [&import](const Segment& candidate) {
			return (static_cast<std::uint8_t>(candidate.permissions) &
				static_cast<std::uint8_t>(ModMemoryPermission::Execute)) != 0 &&
				import.address >= candidate.address &&
				import.address - candidate.address <= candidate.fileSize &&
				candidate.fileSize - (import.address - candidate.address) >= sizeof(std::uint32_t);
		});
		if (segment == segments.end() || import.address % 4 != 0)
		{ error = "a CEX2 import stub is outside file-backed executable memory"; return false; }
		const auto offset = segment->fileOffset + import.address - segment->address;
		if (U32(package.elf, offset) != 0x0400ffffU)
		{ error = "a CEX2 import stub does not contain the required placeholder"; return false; }
		const auto index = osLib_getFunctionIndex("cemuextend", import.name.c_str());
		if (index < 0 || index > std::numeric_limits<std::uint16_t>::max())
		{ error = "a CEX2 HLE import is unavailable"; return false; }
		const auto opcode = 0x04000000U | static_cast<std::uint16_t>(index);
		package.elf[offset] = static_cast<std::byte>(opcode >> 24);
		package.elf[offset + 1] = static_cast<std::byte>(opcode >> 16);
		package.elf[offset + 2] = static_cast<std::byte>(opcode >> 8);
		package.elf[offset + 3] = static_cast<std::byte>(opcode);
	}
	return true;
}

} // namespace

struct CemodRuntime::Impl
{
	static constexpr std::uint64_t kTrustedHandleMask = 1ULL << 63;
	struct Instance
	{
		CemodPackage package;
		std::unique_ptr<ModExecutionContext> context;
		std::unique_ptr<PPCInterpreterGlobal_t> global;
		std::unique_ptr<PPCInterpreter_t> cpu;
		Entrypoints entrypoints;
		std::uint32_t stackTop{};
	};
	mutable std::mutex mutex;
	std::map<std::uint64_t, Instance> mods;
	std::uint64_t nextHandle{1};
	std::uint32_t generation{1};
	TrustedCemodRuntime trusted;
	std::chrono::microseconds titleTime{};
};

CemodRuntime::CemodRuntime() : m_impl(std::make_unique<Impl>()) {}
CemodRuntime::~CemodRuntime() { UnloadAll(); }

std::optional<std::uint64_t> CemodRuntime::Load(CemodPackage package,
	std::uint32_t userPermissions, std::uint32_t titlePermissions, std::string& error,
	const ModServicePermissions* servicePermissions)
{
	if (package.IsTrustedNative())
	{
		const ModServicePermissions services = servicePermissions ? *servicePermissions : ModServicePermissions{};
		if (Size() >= kMaximumModsPerTitle)
			{ error = "the title already has 16 loaded Mods"; return std::nullopt; }
		auto handle = m_impl->trusted.Load(std::move(package), userPermissions & titlePermissions,
			services, error);
		return handle ? std::optional{*handle | Impl::kTrustedHandleMask} : std::nullopt;
	}
	std::unique_lock lock(m_impl->mutex);
	if (m_impl->mods.size() + m_impl->trusted.Size() >= kMaximumModsPerTitle) { error = "the title already has 16 loaded Mods"; return std::nullopt; }
	if (package.principal.empty() || package.elf.empty() || package.manifest.codeBytes == 0 ||
		package.manifest.codeBytes > ModExecutionContext::kMaximumCodeBytes ||
		package.manifest.privateBytes == 0 ||
		package.manifest.privateBytes > ModExecutionContext::kMaximumPrivateBytes ||
		package.manifest.stackBytes == 0 ||
		package.manifest.stackBytes > ModExecutionContext::kMaximumStackBytes ||
		package.manifest.stackBytes % ModExecutionContext::kPageSize != 0 ||
		package.manifest.instructionsPerFrame == 0 ||
		package.manifest.instructionsPerFrame > ModExecutionContext::kMaximumInstructionsPerFrame ||
		package.manifest.timeMicrosecondsPerFrame == 0 ||
		package.manifest.timeMicrosecondsPerFrame > 1000)
	{
		error = "package resource limits are invalid";
		return std::nullopt;
	}
	for (const auto& [handle, mod] : m_impl->mods)
		if (mod.package.principal == package.principal) { error = "this Mod principal is already loaded"; return std::nullopt; }
	std::vector<Segment> segments;
	Entrypoints entrypoints{};
	std::vector<HleImport> imports;
	std::uint32_t base{}, stackBase{}, size{};
	if (!ParseElf(package, segments, entrypoints, imports, base, stackBase, size, error) ||
		!BindHleImports(package, segments, imports, error))
		return std::nullopt;
	const auto handle = m_impl->nextHandle++;
	auto context = std::make_unique<ModExecutionContext>(handle, m_impl->generation++, package.principal, base, size);
	context->SetTitleId(package.targetTitleId);
	context->SetGrantedPermissions(package.manifest.requestedPermissions & userPermissions & titlePermissions);
	if (servicePermissions) context->SetServicePermissions(*servicePermissions);
	for (const auto& segment : segments)
	{
		if (!context->Map(segment.address,
			std::span<const std::byte>(package.elf).subspan(segment.fileOffset, segment.fileSize),
			segment.mappedSize, segment.permissions))
		{
			error = "PPC ELF segments overlap or violate W^X";
			return std::nullopt;
		}
	}
	if (!context->Map(stackBase, {}, package.manifest.stackBytes,
		ModMemoryPermission::Read | ModMemoryPermission::Write))
	{
		error = "Mod stack cannot be mapped";
		return std::nullopt;
	}
	cemuextend_hle::ConfigureCex2HleAccess(*context);
	auto global = std::make_unique<PPCInterpreterGlobal_t>();
	auto cpu = std::make_unique<PPCInterpreter_t>();
	cpu->modExecutionContext = context.get();
	cpu->global = global.get();
	cpu->gpr[1] = stackBase + package.manifest.stackBytes - 16;
	m_impl->mods.emplace(handle, Impl::Instance{std::move(package), std::move(context),
		std::move(global), std::move(cpu), entrypoints, stackBase + package.manifest.stackBytes - 16});
	lock.unlock();
	if (!Invoke(handle, CemodLifecycle::Init))
	{
		std::lock_guard cleanupLock(m_impl->mutex);
		const auto failed = m_impl->mods.find(handle);
		if (failed != m_impl->mods.end())
		{
			cemuextend_hle::Cex2Host::Instance().CloseOwner(*failed->second.context);
			PPCRecompiler_invalidateSandboxContext(failed->second.context->AddressSpaceId(),
				failed->second.context->Generation());
			m_impl->mods.erase(failed);
		}
		error = "cemod_init faulted or exceeded its CPU budget";
		return std::nullopt;
	}
	return handle;
}

bool CemodRuntime::Invoke(std::uint64_t handle, CemodLifecycle lifecycle,
	std::uint32_t argument, std::uint32_t argumentSize)
{
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->mods.find(handle);
	if (found == m_impl->mods.end() || found->second.context->IsStopped() ||
		m_impl->titleTime.count() >= kMaximumTitleTimeMicroseconds)
		return false;
	auto& mod = found->second;
	const auto address = lifecycle == CemodLifecycle::Init ? mod.entrypoints.init :
		lifecycle == CemodLifecycle::Tick ? mod.entrypoints.tick :
		lifecycle == CemodLifecycle::Event ? mod.entrypoints.event : mod.entrypoints.shutdown;
	auto* previous = PPCInterpreter_getCurrentInstance();
	PPCInterpreter_setCurrentInstance(mod.cpu.get());
	mod.cpu->instructionPointer = address;
	mod.cpu->spr.LR = 0;
	mod.cpu->gpr[1] = mod.stackTop;
	mod.cpu->gpr[3] = argument;
	mod.cpu->gpr[4] = argumentSize;
	mod.cpu->gpr[5] = 0;
	mod.cpu->memoryException = false;
	const auto started = std::chrono::steady_clock::now();
	std::uint64_t instructions{};
	ModFaultReason exceeded = ModFaultReason::None;
	while (mod.cpu->instructionPointer != 0 && !mod.context->IsStopped())
	{
		if (instructions >= mod.package.manifest.instructionsPerFrame)
		{
			exceeded = ModFaultReason::InstructionQuota;
			break;
		}
		if (std::chrono::steady_clock::now() - started >
			std::chrono::microseconds(mod.package.manifest.timeMicrosecondsPerFrame))
		{
			exceeded = ModFaultReason::WallClockQuota;
			break;
		}
		mod.cpu->remainingCycles = 1;
		if (!PPCRecompiler_executeSandboxInstruction(mod.cpu.get()))
		{
			mod.context->Stop(ModFaultReason::InvalidMapping,
				mod.cpu->instructionPointer, ModMemoryPermission::Execute);
			break;
		}
		++instructions;
	}
	PPCInterpreter_setCurrentInstance(previous);
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started);
	m_impl->titleTime += elapsed;
	if (mod.cpu->instructionPointer != 0 && !mod.context->IsStopped())
		mod.context->MarkQuotaExceeded(exceeded == ModFaultReason::None ?
			ModFaultReason::WallClockQuota : exceeded);
	return mod.cpu->instructionPointer == 0 && !mod.context->IsStopped();
}

bool CemodRuntime::Unload(std::uint64_t handle)
{
	if ((handle & Impl::kTrustedHandleMask) != 0)
		return m_impl->trusted.Unload(handle & ~Impl::kTrustedHandleMask);
	(void)Invoke(handle, CemodLifecycle::Shutdown);
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->mods.find(handle);
	if (found == m_impl->mods.end()) return false;
	cemuextend_hle::Cex2Host::Instance().CloseOwner(*found->second.context);
	PPCRecompiler_invalidateSandboxContext(found->second.context->AddressSpaceId(),
		found->second.context->Generation());
	m_impl->mods.erase(found);
	return true;
}

void CemodRuntime::BeginFrame()
{
	std::lock_guard lock(m_impl->mutex);
	m_impl->titleTime = {};
	for (auto& [handle, mod] : m_impl->mods) mod.context->BeginFrame();
}

void CemodRuntime::TickAll()
{
	std::vector<std::uint64_t> handles;
	{
		std::lock_guard lock(m_impl->mutex);
		handles.reserve(m_impl->mods.size());
		for (const auto& [handle, mod] : m_impl->mods)
			if (!mod.context->IsStopped()) handles.push_back(handle);
	}
	BeginFrame();
	for (const auto handle : handles)
		(void)Invoke(handle, CemodLifecycle::Tick);
}

void CemodRuntime::EventAll(std::uint32_t event)
{
	std::vector<std::uint64_t> handles;
	{
		std::lock_guard lock(m_impl->mutex);
		for (const auto& [handle, mod] : m_impl->mods)
			if (!mod.context->IsStopped()) handles.push_back(handle);
	}
	for (const auto handle : handles)
		(void)Invoke(handle, CemodLifecycle::Event, event, 0);
}

void CemodRuntime::UpdatePermissions(std::string_view principal, std::uint32_t permissions,
	const ModServicePermissions& services)
{
	std::lock_guard lock(m_impl->mutex);
	for (auto& [handle, mod] : m_impl->mods)
	{
		if (mod.package.principal != principal) continue;
		mod.context->SetServicePermissions(services);
		cemuextend_hle::Cex2Host::Instance().PermissionsChanged(*mod.context,
			mod.package.manifest.requestedPermissions & permissions);
		break;
	}
}

void CemodRuntime::UpdateTitlePermissions(const ModServicePermissions& services)
{
	std::uint32_t trustedPermissions{};
	std::lock_guard lock(m_impl->mutex);
	for (auto& [handle, mod] : m_impl->mods)
	{
		mod.context->SetServicePermissions(services);
		cemuextend_hle::Cex2Host::Instance().PermissionsChanged(*mod.context,
			mod.context->GrantedPermissions());
	}
	if (auto* owner = m_impl->trusted.Owner()) trustedPermissions = owner->GrantedPermissions();
	m_impl->trusted.UpdatePermissions(trustedPermissions, services);
}

void CemodRuntime::UnloadAll()
{
	for (;;) { std::uint64_t handle{}; { std::lock_guard lock(m_impl->mutex); if (m_impl->mods.empty()) break; handle = m_impl->mods.begin()->first; } (void)Unload(handle); }
	m_impl->trusted.UnloadAll();
}

ModExecutionContext* CemodRuntime::Context(std::uint64_t handle)
{
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->mods.find(handle);
	return found == m_impl->mods.end() ? nullptr : found->second.context.get();
}

PPCInterpreter_t* CemodRuntime::Cpu(std::uint64_t handle)
{
	std::lock_guard lock(m_impl->mutex);
	const auto found = m_impl->mods.find(handle);
	return found == m_impl->mods.end() ? nullptr : found->second.cpu.get();
}

std::size_t CemodRuntime::Size() const
{
	std::lock_guard lock(m_impl->mutex);
	return m_impl->mods.size() + m_impl->trusted.Size();
}

cemuextend_hle::Cex2Owner* CemodRuntime::TrustedOwner()
{
	return m_impl->trusted.Owner();
}
