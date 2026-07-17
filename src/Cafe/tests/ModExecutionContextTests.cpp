#include "Cafe/HW/Espresso/ModExecutionContext.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
	[[noreturn]] void CheckFailed(const char* expression, int line)
	{
		std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
		std::abort();
	}

#define CHECK(condition) do { if (!(condition)) CheckFailed(#condition, __LINE__); } while (false)

	constexpr std::uint32_t kBase = 0x10000000;
	constexpr std::uint32_t kSize = 4 * ModExecutionContext::kPageSize;
	const std::array kCode{std::byte{0x60}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

	ModExecutionContext NewContext()
	{
		return {1, 1, "test-principal", kBase, kSize};
	}

	void TestPermissions()
	{
		auto context = NewContext();
		CHECK(context.Map(kBase, kCode, ModExecutionContext::kPageSize,
			ModMemoryPermission::Read | ModMemoryPermission::Execute));
		CHECK(context.Map(kBase + ModExecutionContext::kPageSize, {},
			ModExecutionContext::kPageSize, ModMemoryPermission::Read | ModMemoryPermission::Write));
		CHECK(context.Resolve(kBase, kCode.size(), ModMemoryPermission::Execute) != nullptr);
		CHECK(context.Resolve(kBase + ModExecutionContext::kPageSize, 4,
			ModMemoryPermission::Write) != nullptr);
		CHECK(context.Resolve(kBase, 1, ModMemoryPermission::Write) == nullptr);
		CHECK(context.IsStopped());
		CHECK(context.Fault().reason == ModFaultReason::PermissionDenied);

		auto nx = NewContext();
		CHECK(nx.Map(kBase, {}, ModExecutionContext::kPageSize,
			ModMemoryPermission::Read | ModMemoryPermission::Write));
		CHECK(nx.Resolve(kBase, 4, ModMemoryPermission::Execute) == nullptr);
		CHECK(nx.Fault().reason == ModFaultReason::PermissionDenied);

		auto unmapped = NewContext();
		CHECK(unmapped.Resolve(kBase + 3 * ModExecutionContext::kPageSize, 4,
			ModMemoryPermission::Read) == nullptr);
		CHECK(unmapped.Fault().reason == ModFaultReason::Unmapped);

		auto wx = NewContext();
		CHECK(!wx.Map(kBase, {}, ModExecutionContext::kPageSize,
			ModMemoryPermission::Read | ModMemoryPermission::Write | ModMemoryPermission::Execute));
		CHECK(wx.Fault().reason == ModFaultReason::InvalidMapping);
	}

	void TestHleAndQuotas()
	{
		auto hle = NewContext();
		CHECK(!hle.IsHleAllowed(7));
		hle.AllowHle(7);
		CHECK(hle.IsHleAllowed(7));

		auto instructions = NewContext();
		for (int frame = 0; frame < 3; ++frame)
		{
			instructions.BeginFrame();
			CHECK(instructions.ConsumeInstructions(ModExecutionContext::kMaximumInstructionsPerFrame));
			CHECK(!instructions.ConsumeInstructions());
		}
		CHECK(instructions.Fault().reason == ModFaultReason::InstructionQuota);

		auto wallClock = NewContext();
		for (int frame = 0; frame < 3; ++frame)
		{
			wallClock.BeginFrame();
			CHECK(!wallClock.CheckWallClock(std::chrono::steady_clock::now() + std::chrono::seconds(1)));
		}
		CHECK(wallClock.Fault().reason == ModFaultReason::WallClockQuota);
	}
}

int main()
{
	TestPermissions();
	TestHleAndQuotas();
	std::cout << "ModExecutionContext tests passed\n";
}
