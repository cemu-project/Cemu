#include "Common/precompiled.h"

#include "Cafe/HW/Espresso/CemodRuntime.h"
#include "Cafe/HW/Espresso/ModExecutionContext.h"
#include "Cafe/HW/Espresso/PPCState.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

[[noreturn]] void CheckFailed(const char* expression, int line)
{
	std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
	std::abort();
}
#define CHECK(condition) do { if (!(condition)) CheckFailed(#condition, __LINE__); } while (false)

thread_local PPCInterpreter_t* gCurrentCpu{};

void Be16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::byte>(value >> 8);
	bytes[offset + 1] = static_cast<std::byte>(value);
}

void Be32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
	bytes[offset] = static_cast<std::byte>(value >> 24);
	bytes[offset + 1] = static_cast<std::byte>(value >> 16);
	bytes[offset + 2] = static_cast<std::byte>(value >> 8);
	bytes[offset + 3] = static_cast<std::byte>(value);
}

std::vector<std::byte> LifecycleElf(bool loopingTick = false)
{
	constexpr std::uint32_t codeOffset = 84;
	constexpr std::uint32_t stringsOffset = 100;
	static constexpr char stringBytes[] = "\0cemod_init\0cemod_tick\0cemod_event\0cemod_shutdown\0";
	constexpr std::string_view strings{stringBytes, sizeof(stringBytes) - 1};
	constexpr std::uint32_t symbolsOffset = 152;
	constexpr std::uint32_t sectionsOffset = 232;
	std::vector<std::byte> elf(sectionsOffset + 3 * 40);
	elf[0]=std::byte{0x7f};elf[1]=std::byte{0x45};elf[2]=std::byte{0x4c};elf[3]=std::byte{0x46};
	elf[4]=std::byte{1};elf[5]=std::byte{2};elf[6]=std::byte{1};
	Be16(elf,16,2);Be16(elf,18,20);Be32(elf,20,1);Be32(elf,24,0x10000000);
	Be32(elf,28,52);Be32(elf,32,sectionsOffset);Be16(elf,40,52);Be16(elf,42,32);Be16(elf,44,1);
	Be16(elf,46,40);Be16(elf,48,3);
	Be32(elf,52,1);Be32(elf,56,codeOffset);Be32(elf,60,0x10000000);Be32(elf,64,0x10000000);
	Be32(elf,68,16);Be32(elf,72,16);Be32(elf,76,5);Be32(elf,80,4096);
	for(std::size_t index=0;index<4;++index) Be32(elf,codeOffset+index*4,0x4e800020);
	if(loopingTick) Be32(elf,codeOffset+4,0x48000000);
	std::memcpy(elf.data()+stringsOffset,strings.data(),strings.size());
	const std::array<std::uint32_t,4> names{1,12,23,35};
	for(std::uint32_t index=0;index<4;++index)
	{
		const auto symbol=symbolsOffset+(index+1)*16;
		Be32(elf,symbol,names[index]);Be32(elf,symbol+4,0x10000000+index*4);
		elf[symbol+12]=std::byte{0x12};Be16(elf,symbol+14,1);
	}
	const auto stringsSection=sectionsOffset+40;
	Be32(elf,stringsSection+4,3);Be32(elf,stringsSection+16,stringsOffset);Be32(elf,stringsSection+20,strings.size());
	const auto symbolsSection=sectionsOffset+80;
	Be32(elf,symbolsSection+4,2);Be32(elf,symbolsSection+16,symbolsOffset);Be32(elf,symbolsSection+20,80);
	Be32(elf,symbolsSection+24,1);Be32(elf,symbolsSection+36,16);
	return elf;
}

CemodPackage Package(std::string principal, bool looping = false)
{
	CemodPackage package;
	package.principal=std::move(principal);
	package.targetTitleId=0x0005000012345678ULL;
	package.elf=LifecycleElf(looping);
	package.manifest.modId="test.mod";
	package.manifest.requestedPermissions=7;
	package.manifest.codeBytes=4096;
	package.manifest.privateBytes=4096;
	package.manifest.stackBytes=4096;
	package.manifest.instructionsPerFrame=looping?2:100;
	package.manifest.timeMicrosecondsPerFrame=1000;
	package.manifest.entrypoint="cemod_init";
	return package;
}

void TestLoadLifecycleAndPermissionIntersection()
{
	CemodRuntime runtime;
	std::string error;
	const auto handle=runtime.Load(Package("principal:one"),3,5,error);
	CHECK(handle.has_value());
	CHECK(runtime.Size()==1);
	CHECK(runtime.Context(*handle)->GrantedPermissions()==1);
	CHECK(runtime.Cpu(*handle)->modExecutionContext==runtime.Context(*handle));
	runtime.BeginFrame();
	CHECK(runtime.Invoke(*handle,CemodLifecycle::Tick));
	CHECK(runtime.Invoke(*handle,CemodLifecycle::Event,0,0));
	CHECK(runtime.Unload(*handle));
	CHECK(runtime.Size()==0);
}

void TestLimitsAndIsolation()
{
	CemodRuntime runtime;
	std::string error;
	const auto looping=runtime.Load(Package("principal:loop",true),7,7,error);
	const auto healthy=runtime.Load(Package("principal:healthy"),7,7,error);
	CHECK(looping&&healthy);
	for(int frame=0;frame<3;++frame)
	{
		runtime.BeginFrame();
		CHECK(!runtime.Invoke(*looping,CemodLifecycle::Tick));
		CHECK(runtime.Invoke(*healthy,CemodLifecycle::Tick));
	}
	CHECK(runtime.Context(*looping)->IsStopped());
	CHECK(runtime.Context(*looping)->Fault().reason==ModFaultReason::InstructionQuota);
	CHECK(!runtime.Context(*healthy)->IsStopped());

	for(std::size_t index=runtime.Size();index<CemodRuntime::kMaximumModsPerTitle;++index)
		CHECK(runtime.Load(Package("principal:"+std::to_string(index)),7,7,error).has_value());
	CHECK(!runtime.Load(Package("principal:overflow"),7,7,error).has_value());
	CHECK(runtime.Size()==CemodRuntime::kMaximumModsPerTitle);
}

} // namespace

PPCInterpreter_t* PPCInterpreter_getCurrentInstance(){return gCurrentCpu;}
void PPCInterpreter_setCurrentInstance(PPCInterpreter_t* cpu){gCurrentCpu=cpu;}
void PPCInterpreterSlim_executeInstruction(PPCInterpreter_t* cpu)
{
	auto* bytes=cpu->modExecutionContext->Resolve(cpu->instructionPointer,4,ModMemoryPermission::Execute);
	if(!bytes)return;
	const auto instruction=(std::to_integer<std::uint32_t>(bytes[0])<<24)|
		(std::to_integer<std::uint32_t>(bytes[1])<<16)|(std::to_integer<std::uint32_t>(bytes[2])<<8)|
		std::to_integer<std::uint32_t>(bytes[3]);
	if(instruction==0x4e800020)cpu->instructionPointer=cpu->spr.LR;
	else if(instruction!=0x48000000)cpu->modExecutionContext->Stop(ModFaultReason::InvalidMapping,cpu->instructionPointer,ModMemoryPermission::Execute);
}

namespace cemuextend_hle { void ConfigureCex2HleAccess(ModExecutionContext&) {} }

int main()
{
	TestLoadLifecycleAndPermissionIntersection();
	TestLimitsAndIsolation();
	return 0;
}
