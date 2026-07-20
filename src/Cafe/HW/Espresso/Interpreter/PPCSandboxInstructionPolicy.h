#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace cemod_sandbox
{
	constexpr std::uint32_t Bits(std::uint32_t value, std::uint32_t index, std::uint32_t count)
	{
		return (value >> (31U - index)) & ((1U << count) - 1U);
	}

	template<typename T, std::size_t Size>
	constexpr bool Contains(T value, const std::array<T, Size>& values)
	{
		return std::ranges::find(values, value) != values.end();
	}

	// Only instructions implemented by the checked user-mode interpreter are
	// admitted. Host debugger, MMU, GPU-cache, code-invalidation and supervisor
	// operations are deliberately absent.
	constexpr bool IsInstructionAllowed(std::uint32_t opcode)
	{
		const auto primary = opcode >> 26;
		if (primary == 4)
		{
			const auto subcategory = Bits(opcode, 30, 5);
			if (subcategory == 0)
				return Contains(Bits(opcode, 25, 5), std::array{0U, 1U, 2U});
			if (subcategory == 8)
				return Contains(Bits(opcode, 25, 5), std::array{1U, 2U, 4U, 8U});
			if (subcategory == 16)
				return Contains(Bits(opcode, 25, 5), std::array{16U, 17U, 18U, 19U});
			return Contains(subcategory,
				std::array{6U, 7U, 10U, 11U, 12U, 13U, 14U, 15U, 18U,
					20U, 21U, 22U, 23U, 24U, 25U, 26U, 28U, 29U, 30U, 31U});
		}
		if (primary == 19)
			return Contains(Bits(opcode, 30, 10),
				std::array{0U, 16U, 33U, 129U, 150U, 193U, 225U, 257U,
					289U, 417U, 449U, 528U});
		if (primary == 31)
		{
			const auto extended = Bits(opcode, 30, 10);
			if (extended == 339 || extended == 467)
			{
				const auto spr = ((opcode >> 16) & 0x1fU) | (((opcode >> 11) & 0x1fU) << 5U);
				constexpr std::uint32_t xer = 1, lr = 8, ctr = 9, upir = 1023;
				constexpr std::uint32_t ugqr0 = 912, ugqr7 = 919;
				const bool common = spr == lr || spr == ctr || spr == xer ||
					(spr >= ugqr0 && spr <= ugqr7);
				return common || (extended == 339 && spr == upir);
			}
			if (extended == 371)
			{
				const auto spr = ((opcode >> 16) & 0x1fU) | (((opcode >> 11) & 0x1fU) << 5U);
				return spr == 268 || spr == 269;
			}
			return Contains(extended, std::array{
				0U, 8U, 10U, 11U, 19U, 20U, 23U, 24U, 26U, 28U, 32U, 40U,
				55U, 60U, 75U, 87U, 104U, 119U, 124U, 136U, 138U, 144U,
				150U, 151U, 183U, 200U, 202U, 215U, 232U, 234U, 235U, 247U,
				266U, 278U, 279U, 284U, 311U, 316U, 343U, 375U, 407U, 412U,
				439U, 444U, 459U, 470U, 476U, 491U, 512U, 520U, 522U, 523U,
				533U, 534U, 535U, 536U, 552U, 567U, 587U, 597U, 598U, 599U,
				616U, 631U, 648U, 650U, 661U, 662U, 663U, 695U, 712U, 714U,
				725U, 727U, 744U, 746U, 747U, 759U, 778U, 790U, 792U, 824U,
				854U, 918U, 922U, 954U, 971U, 983U, 1003U, 1014U});
		}
		if (primary == 59)
			return Contains(Bits(opcode, 30, 5),
				std::array{18U, 20U, 21U, 24U, 25U, 28U, 29U, 30U, 31U});
		if (primary == 63)
		{
			if (Contains(Bits(opcode, 30, 5),
				std::array{0U, 12U, 15U, 18U, 20U, 21U, 23U, 25U, 26U, 28U, 29U, 30U, 31U}))
				return true;
			return Contains(Bits(opcode, 30, 10),
				std::array{14U, 32U, 38U, 40U, 72U, 136U, 264U, 583U, 711U});
		}
		return Contains(primary, std::array{
			1U, 3U, 7U, 8U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 18U,
			20U, 21U, 23U, 24U, 25U, 26U, 27U, 28U, 29U,
			32U, 33U, 34U, 35U, 36U, 37U, 38U, 39U, 40U, 41U, 42U, 43U,
			44U, 45U, 46U, 47U, 48U, 49U, 50U, 51U, 52U, 53U, 54U, 55U,
			56U, 57U, 60U, 61U});
	}
}
