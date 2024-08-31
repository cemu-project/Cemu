#include "RecompilerTests.h"
#include "Common/precompiled.h"
#include "HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "HW/Espresso/Interpreter/PPCInterpreterHelper.h"
#include "HW/Espresso/PPCState.h"
#include "HW/Espresso/Recompiler/IML/IMLInstruction.h"
#include "HW/Espresso/Recompiler/PPCRecompiler.h"
#include "HW/Espresso/Recompiler/PPCRecompilerIml.h"
#include "BackendX64/BackendX64.h"
#include "HW/MMU/MMU.h"
#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <random>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <variant>

#if defined(__aarch64__)
#include "BackendAArch64/BackendAArch64.h"
constexpr inline uint32 floatRegStartIndex = 0;
#else
constexpr inline uint32 floatRegStartIndex = 16;
#endif

constexpr inline size_t test_memory_base_size = 4096;
template<typename T>
T relativeDiff(T a, T b)
{
	T min_val = std::numeric_limits<T>::min();
	T max_val = std::numeric_limits<T>::max();
	if ((std::isnan)(a) || (std::isnan)(b))
		return max_val;
	if (fabs(b) > max_val)
	{
		if (fabs(a) > max_val)
			return (a < 0) == (b < 0) ? 0 : max_val;
		else
			return max_val;
	}
	else if (fabs(a) > max_val)
		return max_val;

	if (((a < 0) != (b < 0)) && (a != 0) && (b != 0))
		return max_val;
	a = fabs(a);
	b = fabs(b);
	if (a < min_val)
		a = min_val;
	if (b < min_val)
		b = min_val;

	T relativeDiff = (std::max)(fabs((a - b) / a), fabs((a - b) / b));
	if (max_val * std::numeric_limits<T>::epsilon() < relativeDiff)
		return max_val;
	T epsilonDiff = relativeDiff / std::numeric_limits<T>::epsilon();
	return (std::max)(fabs((a - b) / a), fabs((a - b) / b));
}
template<typename T>
T epsilonDiff(T a, T b)
{
	T r = relativeDiff(a, b);

	if (std::numeric_limits<T>::max() * std::numeric_limits<T>::epsilon() < r)
		return std::numeric_limits<T>::max();
	return r / std::numeric_limits<T>::epsilon();
}
template<typename T>
	requires std::is_floating_point_v<T>
bool fp_equal(T a, T b)
{
	return epsilonDiff(a, b) <= std::numeric_limits<T>::epsilon();
}

void assertUninitializedMemory(size_t ignoreIndexStart = 0, size_t bytesCount = 0)
{
	for (size_t i = 0; i < test_memory_base_size; i++)
	{
		if (ignoreIndexStart <= i && i < ignoreIndexStart + bytesCount)
			continue;
		uint8 memValue = memory_base[i];
		cemu_assert_debug(memValue == 0);
	}
}

// using emit_inst_fn = std::function<IMLInstruction&()>;
class emit_inst
{
	ppcImlGenContext_t* m_imlGenContext;

  public:
	emit_inst(ppcImlGenContext_t* imlGenContext)
		: m_imlGenContext(imlGenContext) {}
	IMLInstruction& operator()()
	{
		return m_imlGenContext->emitInst();
	}
};
using emit_inst_fn = emit_inst&;
using setup_fn = std::function<void(emit_inst_fn, PPCInterpreter_t&)>;
using verify_fn = std::function<void(PPCInterpreter_t&)>;
using setup_and_verify_fn = std::function<void(setup_fn, verify_fn)>;

using test_fn = std::function<void(setup_and_verify_fn)>;
std::mt19937 gen(std::random_device{}());
std::uniform_int_distribution<sint32> distrib_sint32(std::numeric_limits<sint32>::min(), std::numeric_limits<sint32>::max());
std::uniform_int_distribution<uint32> distrib_uint32(std::numeric_limits<uint32>::min(), std::numeric_limits<uint32>::max());
std::uniform_real_distribution<double> distrib_double(std::numeric_limits<double>::min(), std::numeric_limits<double>::max());

struct iota_view_single : public std::ranges::iota_view<int, int>
{
	iota_view_single(int value)
		: std::ranges::iota_view<int, int>(value, value + 1) {}
};
std::initializer_list<std::ranges::iota_view<int, int>> namesI64{
	std::ranges::iota_view<int, int>(PPCREC_NAME_R0, PPCREC_NAME_R0 + 32),
	std::ranges::iota_view<int, int>(PPCREC_NAME_SPR0 + SPR_UGQR0, PPCREC_NAME_SPR0 + SPR_UGQR7 + 1),
	iota_view_single(PPCREC_NAME_SPR0 + SPR_LR),
	iota_view_single(PPCREC_NAME_SPR0 + SPR_CTR),
	iota_view_single(PPCREC_NAME_SPR0 + SPR_XER),
	std::ranges::iota_view<int, int>(PPCREC_NAME_CR, PPCREC_NAME_CR_LAST + 1),
	std::ranges::iota_view<int, int>(PPCREC_NAME_TEMPORARY, PPCREC_NAME_TEMPORARY + 4),
	std::ranges::iota_view<int, int>(PPCREC_NAME_TEMPORARY, PPCREC_NAME_TEMPORARY + 4),
	iota_view_single(PPCREC_NAME_XER_CA),
	iota_view_single(PPCREC_NAME_XER_SO),
	iota_view_single(PPCREC_NAME_CPU_MEMRES_EA),
	iota_view_single(PPCREC_NAME_CPU_MEMRES_VAL),
};

void r_s32_tests(setup_and_verify_fn setupAndVerify)
{
	sint32 value = distrib_sint32(gen);

	setupAndVerify(
		[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
			IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
			IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
			emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regR32, value);
			emitInst().make_name_r(PPCREC_NAME_R0, regR64);
		},
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == value);
		});

	setupAndVerify(
		[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
			IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
			IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
			emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regR32, value);
			emitInst().make_r_s32(PPCREC_IML_OP_LEFT_ROTATE, regR32, 20);
			emitInst().make_name_r(PPCREC_NAME_R0, regR64);
		},
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == std::rotl((uint32)value, 20));
		});
	setupAndVerify(
		[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
			IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
			IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
			emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, regR32, value);
			emitInst().make_r_s32(PPCREC_IML_OP_LEFT_ROTATE, regR32, 35);
			emitInst().make_name_r(PPCREC_NAME_R0, regR64);
		},
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == std::rotl((uint32)value, 35));
		});
}

void conditional_r_s32_tests(setup_and_verify_fn setupAndVerify)
{
	// todo
}

void r_r_s32_tests(setup_and_verify_fn setupAndVerify)
{
	sint32 immS32 = distrib_sint32(gen);
	uint32 regAValue = distrib_uint32(gen);
	uint32 regRValue = distrib_uint32(gen);
	auto runTest = [&](uint32 operation, verify_fn verifyFn) {
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regA64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regA32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				hCPU.gpr[1] = regAValue;
				hCPU.gpr[2] = regRValue;
				emitInst().make_r_name(regA64, PPCREC_NAME_R0 + 1);
				emitInst().make_r_name(regR64, PPCREC_NAME_R0 + 2);
				emitInst().make_r_r_s32(operation, regR32, regA32, immS32);
				emitInst().make_name_r(PPCREC_NAME_R0, regR64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regA64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[1] == regAValue);
				verifyFn(hCPU);
			});
	};

	runTest(
		PPCREC_IML_OP_ADD,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == regAValue + immS32);
		});
	runTest(
		PPCREC_IML_OP_SUB,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == regAValue - immS32);
		});
	runTest(
		PPCREC_IML_OP_AND,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == (regAValue & immS32));
		});
	runTest(
		PPCREC_IML_OP_OR,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == (regAValue | immS32));
		});
	runTest(
		PPCREC_IML_OP_MULTIPLY_SIGNED,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == ((sint32)regAValue * immS32));
		});
	runTest(
		PPCREC_IML_OP_XOR,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == (regAValue ^ immS32));
		});
	// shifts
	immS32 = immS32 & 0x1F;
	runTest(
		PPCREC_IML_OP_LEFT_SHIFT,
		[&](PPCInterpreter_t& hCPU) {
			uint32 expectedValue = regAValue;
			expectedValue = expectedValue << immS32;
			cemu_assert_debug(hCPU.gpr[0] == expectedValue);
		});
	runTest(
		PPCREC_IML_OP_RIGHT_SHIFT_U,
		[&](PPCInterpreter_t& hCPU) {
			uint32 expectedValue = regAValue;
			expectedValue = expectedValue >> immS32;
			cemu_assert_debug(hCPU.gpr[0] == expectedValue);
		});
	runTest(
		PPCREC_IML_OP_RIGHT_SHIFT_S,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == ((sint32)regAValue >> immS32));
		});
	immS32 = (/*mb*/ 0) | (/*me*/ 0x1D << 8) | (/*sh*/ 3 << 16);
	runTest(
		PPCREC_IML_OP_RLWIMI,
		[&](PPCInterpreter_t& hCPU) {
			uint32 vImm = (uint32)immS32;
			uint32 mb = (vImm >> 0) & 0xFF;
			uint32 me = (vImm >> 8) & 0xFF;
			uint32 sh = ((vImm >> 16) & 0xFF);
			uint32 mask = ppc_mask(mb, me);
			uint32 expectedValue = (regRValue & ~mask) | (std::rotl(regAValue, sh & 0x1F) & mask);
			cemu_assert_debug(hCPU.gpr[0] == expectedValue);
		});
}

void r_r_s32_carry_tests(setup_and_verify_fn setupAndVerify)
{
	using setup_data = std::tuple<sint32, uint32, uint32>;
	auto runTest = [&](uint32 operation, verify_fn verifyFn, std::function<setup_data()> data) {
		auto [regAValue, immS32, regCarryValue] = data();
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regA64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regA32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regCarry64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regCarry32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);
				hCPU.gpr[0] = regAValue;
				hCPU.gpr[1] = regCarryValue;
				emitInst().make_r_name(regA64, PPCREC_NAME_R0);
				emitInst().make_r_name(regCarry64, PPCREC_NAME_R0 + 1);
				emitInst().make_r_r_s32_carry(operation, regR32, regA32, immS32, regCarry32);
				emitInst().make_name_r(PPCREC_NAME_R0, regA64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 2, regR64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 3, regCarry64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] == regAValue);
				verifyFn(hCPU);
			});
	};

	runTest(
		PPCREC_IML_OP_ADD,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[2] == 20);
			cemu_assert_debug(hCPU.gpr[3] == 1);
		},
		[]() -> setup_data {
			return {2147483670, 2147483646, 0};
		});
	runTest(
		PPCREC_IML_OP_ADD,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[2] == 150);
			cemu_assert_debug(hCPU.gpr[3] == 0);
		},
		[]() -> setup_data {
			return {100, 50, 0};
		});
	runTest(
		PPCREC_IML_OP_ADD_WITH_CARRY,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[2] == 21);
			cemu_assert_debug(hCPU.gpr[3] == 1);
		},
		[]() -> setup_data {
			return {2147483670, 2147483646, 1};
		});
	runTest(
		PPCREC_IML_OP_ADD_WITH_CARRY,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[2] == 20);
			cemu_assert_debug(hCPU.gpr[3] == 1);
		},
		[]() -> setup_data {
			return {2147483670, 2147483646, 0};
		});
	runTest(
		PPCREC_IML_OP_ADD_WITH_CARRY,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[2] == 150);
			cemu_assert_debug(hCPU.gpr[3] == 0);
		},
		[]() -> setup_data {
			return {100, 50, 0};
		});
}

void name_r_tests(setup_and_verify_fn setupAndVerify)
{
	auto getValueFromHCPU = [](PPCInterpreter_t& hCPU, IMLName name) -> std::variant<uint32, uint8, FPR_t> {
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
			return hCPU.gpr[name - PPCREC_NAME_R0];
		if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			uint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
				return hCPU.spr.LR;
			else if (sprIndex == SPR_CTR)
				return hCPU.spr.CTR;
			else if (sprIndex == SPR_XER)
				return hCPU.spr.XER;
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
				return hCPU.spr.UGQR[sprIndex - SPR_UGQR0];
		}
		if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
			return hCPU.temporaryGPR_reg[name - PPCREC_NAME_TEMPORARY];
		if (name == PPCREC_NAME_XER_CA)
			return hCPU.xer_ca;
		if (name == PPCREC_NAME_XER_SO)
			return hCPU.xer_so;
		if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
			return hCPU.cr[name - PPCREC_NAME_CR];
		if (name == PPCREC_NAME_CPU_MEMRES_EA)
			return hCPU.reservedMemAddr;
		if (name == PPCREC_NAME_CPU_MEMRES_VAL)
			return hCPU.reservedMemValue;
		if (name >= PPCREC_NAME_FPR0 && name < (PPCREC_NAME_FPR0 + 32))
			return hCPU.fpr[name - PPCREC_NAME_FPR0];
		if (name >= PPCREC_NAME_TEMPORARY_FPR0 || name < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
			return hCPU.temporaryFPR[name - PPCREC_NAME_TEMPORARY_FPR0];
		throw std::invalid_argument(fmt::format("invalid value for name {}", name));
	};

	auto runTest = [&](IMLName name) {
		sint32 value = distrib_sint32(gen);
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg reg64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg reg32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				emitInst().make_r_s32(PPCREC_IML_OP_ASSIGN, reg32, value);
				emitInst().make_name_r(name, reg64);
			},
			[&](PPCInterpreter_t& hCPU) {
				auto hcpuValue = getValueFromHCPU(hCPU, name);
				if (std::holds_alternative<uint32>(hcpuValue))
					cemu_assert_debug((sint32)std::get<uint32>(hcpuValue) == value);
				else if (std::holds_alternative<uint8>(hcpuValue))
					cemu_assert_debug(std::get<uint8>(hcpuValue) == *(uint8*)&value);
				else
					throw std::runtime_error(fmt::format("unexpected value for IMLName {}", name));
			});
	};
	for (auto name : std::views::join(namesI64))
	{
		runTest(name);
	}

	std::initializer_list<std::pair<int, int>> namesF64Pairs = {
		{PPCREC_NAME_FPR0, PPCREC_NAME_FPR0 + 1},
		{PPCREC_NAME_TEMPORARY_FPR0, PPCREC_NAME_FPR0 + 1},
		{PPCREC_NAME_FPR0, PPCREC_NAME_TEMPORARY_FPR0},
	};

	for (auto [src, dest] : namesF64Pairs)
	{
		double fp0 = distrib_double(gen);
		double fp1 = distrib_double(gen);
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				if (src >= PPCREC_NAME_FPR0 && src < (PPCREC_NAME_FPR0 + 32))
					hCPU.fpr[src - PPCREC_NAME_FPR0] = {.fp0 = fp0, .fp1 = fp1};
				else if (src >= PPCREC_NAME_TEMPORARY_FPR0 && src < (PPCREC_NAME_TEMPORARY_FPR0 + 8))
					hCPU.temporaryFPR[src - PPCREC_NAME_TEMPORARY_FPR0] = {.fp0 = fp0, .fp1 = fp1};
				IMLReg reg64 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				emitInst().make_r_name(reg64, src);
				emitInst().make_name_r(dest, reg64);
			},
			[&](PPCInterpreter_t& hCPU) {
				auto hcpuValue = getValueFromHCPU(hCPU, dest);
				if (!std::holds_alternative<FPR_t>(hcpuValue))
					throw std::runtime_error(fmt::format("unexpected value for IMLName {}", dest));
				auto fpr = std::get<FPR_t>(hcpuValue);
				cemu_assert_debug(fpr.fp0 == fp0);
				cemu_assert_debug(fpr.fp1 == fp1);
			});
	}
}

void r_name_tests(setup_and_verify_fn setupAndVerify)
{
	auto setValueForHCPU = [](PPCInterpreter_t& hCPU, IMLName name, uint32 value) {
		if (name >= PPCREC_NAME_R0 && name < PPCREC_NAME_R0 + 32)
		{
			hCPU.gpr[name - PPCREC_NAME_R0] = value;
			return;
		}
		if (name >= PPCREC_NAME_SPR0 && name < PPCREC_NAME_SPR0 + 999)
		{
			uint32 sprIndex = (name - PPCREC_NAME_SPR0);
			if (sprIndex == SPR_LR)
			{
				hCPU.spr.LR = value;
				return;
			}
			else if (sprIndex == SPR_CTR)
			{
				hCPU.spr.CTR = value;
				return;
			}
			else if (sprIndex == SPR_XER)
			{
				hCPU.spr.XER = value;
				return;
			}
			else if (sprIndex >= SPR_UGQR0 && sprIndex <= SPR_UGQR7)
			{
				hCPU.spr.UGQR[sprIndex - SPR_UGQR0] = value;
				return;
			}
		}
		if (name >= PPCREC_NAME_TEMPORARY && name < PPCREC_NAME_TEMPORARY + 4)
		{
			hCPU.temporaryGPR_reg[name - PPCREC_NAME_TEMPORARY] = value;
			return;
		}
		if (name == PPCREC_NAME_XER_CA)
		{
			hCPU.xer_ca = *(uint8*)&value;
			return;
		}
		if (name == PPCREC_NAME_XER_SO)
		{
			hCPU.xer_so = *(uint8*)&value;
			return;
		}
		if (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST)
		{
			hCPU.cr[name - PPCREC_NAME_CR] = *(uint8*)&value;
			return;
		}
		if (name == PPCREC_NAME_CPU_MEMRES_EA)
		{
			hCPU.reservedMemAddr = value;
			return;
		}
		if (name == PPCREC_NAME_CPU_MEMRES_VAL)
		{
			hCPU.reservedMemValue = value;
			return;
		}
		throw std::invalid_argument(fmt::format("invalid value for name {}", name));
	};
	auto runTest = [&](IMLName name) {
		bool isByteValue = name == PPCREC_NAME_XER_CA ||
						   name == PPCREC_NAME_XER_SO ||
						   (name >= PPCREC_NAME_CR && name <= PPCREC_NAME_CR_LAST);
		uint32 value = distrib_uint32(gen);
		std::cout << value << std::endl;
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg reg64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				setValueForHCPU(hCPU, name, value);
				emitInst().make_r_name(reg64, name);
				emitInst().make_name_r(PPCREC_NAME_R0, reg64);
			},
			[&](PPCInterpreter_t& hCPU) {
				if (isByteValue)
					cemu_assert_debug(hCPU.gpr[0] == *(uint8*)&value);
				else
					cemu_assert_debug(hCPU.gpr[0] == value);
			});
	};
	for (auto name : std::views::join(namesI64))
	{
		if (name == PPCREC_NAME_R0) // Used for verifying
			continue;
		runTest(name);
	}
}

void r_r_tests(setup_and_verify_fn setupAndVerify)
{
	auto runTest = [&](uint32 operation, uint32 regAValue, verify_fn verifyFn) {
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regA32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regA64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				hCPU.gpr[0] = regAValue;
				emitInst().make_r_name(regA64, PPCREC_NAME_R0);
				emitInst().make_r_r(operation, regR32, regA32);
				emitInst().make_name_r(PPCREC_NAME_R0, regA64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regR64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] == regAValue);
				verifyFn(hCPU);
			});
	};
	uint32 testValue = distrib_uint32(gen);

	runTest(
		PPCREC_IML_OP_ASSIGN,
		testValue,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[1] == testValue);
		});
	runTest(
		PPCREC_IML_OP_ENDIAN_SWAP,
		testValue,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[1] == _swapEndianU32(testValue));
		});
	runTest(
		PPCREC_IML_OP_ASSIGN_S8_TO_S32,
		230,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(*(sint32*)&hCPU.gpr[1] == -26);
		});
	runTest(
		PPCREC_IML_OP_ASSIGN_S16_TO_S32,
		65000,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(*(sint32*)&hCPU.gpr[1] == -536);
		});
	runTest(
		PPCREC_IML_OP_NOT,
		testValue,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[1] == ~testValue);
		});
	runTest(
		PPCREC_IML_OP_NEG,
		testValue,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[1] == -testValue);
		});
	runTest(
		PPCREC_IML_OP_CNTLZW,
		testValue,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[1] == std::countl_zero(testValue));
		});
	// TODO: PPCREC_IML_OP_DCBZ
}

void r_r_r_tests(setup_and_verify_fn setupAndVerify)
{
	uint32 regAValue = distrib_uint32(gen);
	uint32 regBValue = distrib_uint32(gen);

	auto runTest = [&](uint32 operation, verify_fn verifyFn) {
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regA32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regA64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regB32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);
				IMLReg regB64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				hCPU.gpr[1] = regAValue;
				hCPU.gpr[2] = regBValue;
				emitInst().make_r_name(regA64, PPCREC_NAME_R0 + 1);
				emitInst().make_r_name(regB64, PPCREC_NAME_R0 + 2);
				emitInst().make_r_r_r(operation, regR32, regA32, regB32);
				emitInst().make_name_r(PPCREC_NAME_R0, regR64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regA64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 2, regB64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[1] == regAValue);
				cemu_assert_debug(hCPU.gpr[2] == regBValue);
				verifyFn(hCPU);
			});
	};
	runTest(
		PPCREC_IML_OP_ADD,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == regAValue + regBValue);
		});
	runTest(
		PPCREC_IML_OP_SUB,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == regAValue - regBValue);
		});
	runTest(
		PPCREC_IML_OP_OR,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == (regAValue | regBValue));
		});
	runTest(
		PPCREC_IML_OP_AND,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == (regAValue & regBValue));
		});
	runTest(
		PPCREC_IML_OP_MULTIPLY_SIGNED,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == (sint32)regAValue * (sint32)regBValue);
		});
	runTest(
		PPCREC_IML_OP_DIVIDE_SIGNED,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == (sint32)regAValue / (sint32)regBValue);
		});
	runTest(
		PPCREC_IML_OP_DIVIDE_UNSIGNED,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == regAValue / regBValue);
		});
	regAValue = 232132132;
	regBValue = 12313122;
	runTest(
		PPCREC_IML_OP_MULTIPLY_HIGH_SIGNED,
		[&](PPCInterpreter_t& hCPU) {
			sint32 regAValueSigned = (sint32)regAValue;
			sint32 regBValueSigned = (sint32)regBValue;
			sint64 expected = (sint64)regAValueSigned * regBValueSigned;
			uint32 expected_hi = expected >> 32;
			cemu_assert_debug(hCPU.gpr[0] == expected_hi);
		});
	runTest(
		PPCREC_IML_OP_MULTIPLY_HIGH_UNSIGNED,
		[&](PPCInterpreter_t& hCPU) {
			uint64 expected = (uint64)regAValue * regBValue;
			uint32 expected_hi = expected >> 32;
			cemu_assert_debug(hCPU.gpr[0] == expected_hi);
		});
	regBValue = 31;
	runTest(
		PPCREC_IML_OP_SLW,
		[&](PPCInterpreter_t& hCPU) {
			uint64 expected64 = (uint64)regAValue << (uint64)regBValue;
			uint32 expected32 = (uint32)expected64 & (uint32)expected64;
			cemu_assert_debug(hCPU.gpr[0] == expected32);
		});
	runTest(
		PPCREC_IML_OP_SRW,
		[&](PPCInterpreter_t& hCPU) {
			uint64 expected64 = (uint64)regAValue >> (uint64)regBValue;
			uint32 expected32 = (uint32)expected64;
			cemu_assert_debug(hCPU.gpr[0] == expected32);
		});
	runTest(
		PPCREC_IML_OP_LEFT_ROTATE,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == std::rotl(regAValue, regBValue));
		});
	runTest(
		PPCREC_IML_OP_LEFT_SHIFT,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == regAValue << regBValue);
		});
	runTest(
		PPCREC_IML_OP_RIGHT_SHIFT_U,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == regAValue >> regBValue);
		});
	runTest(
		PPCREC_IML_OP_RIGHT_SHIFT_S,
		[&](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[0] == ((sint32)regAValue >> regBValue));
		});
}

void r_r_r_carry_tests(setup_and_verify_fn setupAndVerify)
{
	using setup_data = std::tuple<uint32, uint32, uint32>;
	auto runTest = [&](uint32 operation, verify_fn verifyFn, std::function<setup_data()> data) {
		auto [regAValue, regBValue, regCarryValue] = data();
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regA64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regA32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regB64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regB32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);
				IMLReg regCarry64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 3);
				IMLReg regCarry32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 3);
				hCPU.gpr[0] = regAValue;
				hCPU.gpr[1] = regBValue;
				hCPU.gpr[2] = regCarryValue;
				emitInst().make_r_name(regA64, PPCREC_NAME_R0);
				emitInst().make_r_name(regB64, PPCREC_NAME_R0 + 1);
				emitInst().make_r_name(regCarry64, PPCREC_NAME_R0 + 2);
				emitInst().make_r_r_r_carry(operation, regR32, regA32, regB32, regCarry32);
				emitInst().make_name_r(PPCREC_NAME_R0, regA64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regB64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 4, regCarry64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 3, regR64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] == regAValue);
				cemu_assert_debug(hCPU.gpr[1] == regBValue);
				verifyFn(hCPU);
			});
	};

	runTest(
		PPCREC_IML_OP_ADD,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[3] == 20);
			cemu_assert_debug(hCPU.gpr[4] == 1);
		},
		[]() -> setup_data {
			return {2147483670, 2147483646, 0};
		});
	runTest(
		PPCREC_IML_OP_ADD,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[3] == 150);
			cemu_assert_debug(hCPU.gpr[4] == 0);
		},
		[]() -> setup_data {
			return {100, 50, 0};
		});
	runTest(
		PPCREC_IML_OP_ADD_WITH_CARRY,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[3] == 21);
			cemu_assert_debug(hCPU.gpr[4] == 1);
		},
		[]() -> setup_data {
			return {2147483670, 2147483646, 1};
		});
	runTest(
		PPCREC_IML_OP_ADD_WITH_CARRY,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[3] == 20);
			cemu_assert_debug(hCPU.gpr[4] == 1);
		},
		[]() -> setup_data {
			return {2147483670, 2147483646, 0};
		});
	runTest(
		PPCREC_IML_OP_ADD_WITH_CARRY,
		[](PPCInterpreter_t& hCPU) {
			cemu_assert_debug(hCPU.gpr[3] == 150);
			cemu_assert_debug(hCPU.gpr[4] == 0);
		},
		[]() -> setup_data {
			return {100, 50, 0};
		});
}

void compare_and_compare_s32_tests(setup_and_verify_fn setupAndVerify)
{
	using test_data_t = std::tuple<sint32, sint32, IMLCondition, bool>;
	std::initializer_list<test_data_t> testData = {
		{100, 100, IMLCondition::EQ, true},
		{100, 101, IMLCondition::EQ, false},
		{100, 100, IMLCondition::NEQ, false},
		{100, 101, IMLCondition::NEQ, true},
		{101, 100, IMLCondition::UNSIGNED_GT, true},
		{-100, 100, IMLCondition::UNSIGNED_GT, true},
		{100, 101, IMLCondition::UNSIGNED_GT, false},
		{100, 100, IMLCondition::UNSIGNED_GT, false},
		{101, 100, IMLCondition::UNSIGNED_LT, false},
		{-100, 100, IMLCondition::UNSIGNED_LT, false},
		{100, 101, IMLCondition::UNSIGNED_LT, true},
		{100, 100, IMLCondition::UNSIGNED_LT, false},
		{100, 101, IMLCondition::SIGNED_LT, true},
		{101, 100, IMLCondition::SIGNED_LT, false},
		{-100, 100, IMLCondition::SIGNED_LT, true},
		{100, 100, IMLCondition::SIGNED_LT, false},
		{100, 101, IMLCondition::SIGNED_GT, false},
		{101, 100, IMLCondition::SIGNED_GT, true},
		{-100, 100, IMLCondition::SIGNED_GT, false},
		{100, 100, IMLCondition::SIGNED_GT, false},
	};

	auto runTest = [&](test_data_t data, bool isRegInstr) {
		auto [regAValue, regBValue, cond, expectedValue] = data;
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regR64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regR32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regA64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regA32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regB64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regB32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);
				hCPU.gpr[0] = regAValue;
				emitInst().make_r_name(regA64, PPCREC_NAME_R0);
				if (isRegInstr)
				{
					hCPU.gpr[1] = regBValue;
					emitInst().make_r_name(regB64, PPCREC_NAME_R0 + 1);
					emitInst().make_compare(regA32, regB32, regR32, cond);
					emitInst().make_name_r(PPCREC_NAME_R0 + 1, regB64);
				}
				else
				{
					emitInst().make_compare_s32(regA32, regBValue, regR32, cond);
				}
				emitInst().make_name_r(PPCREC_NAME_R0, regA64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 2, regR64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] == regAValue);
				cemu_assert_debug(hCPU.gpr[1] == regBValue);
				cemu_assert_debug(hCPU.gpr[2] == expectedValue);
			});
	};
	for (auto&& data : testData)
	{
		runTest(data, true);
		runTest(data, false);
	}
}

template<typename T>
inline constexpr bool is_mem_value = (std::is_same_v<T, uint32> | std::is_same_v<T, uint16> | std::is_same_v<T, uint8>);

template<typename T>
	requires is_mem_value<T>
T getMemoryValue(uint32 memoryIndex)
{
	return *(T*)(memory_base + memoryIndex);
}
sint32 extendSign(uint16 val)
{
	return (sint16)val;
}
sint32 extendSign(uint8 val)
{
	return (sint8)val;
}
sint32 extendSign(uint32 val)
{
	return (sint32)val;
}

template<typename T>
	requires is_mem_value<T>
void assertLoadMemValue(T value, bool signExtend, bool switchEndian, uint32 actualValue)
{
	T expectedValue = value;
	if (switchEndian)
		expectedValue = SwapEndian<T>(value);
	uint32 expectedValue32;
	if (signExtend && sizeof(T) != sizeof(uint32))
		expectedValue32 = extendSign(expectedValue);
	else
		expectedValue32 = expectedValue;
	cemu_assert_debug(actualValue == expectedValue32);
}

template<typename T>
	requires is_mem_value<T>
void setMemoryValue(uint64 memoryIndex, T value)
{
	*(T*)(memory_base + memoryIndex) = value;
}

void load_tests(setup_and_verify_fn setupAndVerify)
{
	using test_data_t = std::tuple<sint32, uint32, bool, bool, std::variant<uint32, uint16, uint8>>;
	using test_case_t = std::tuple<sint32, uint32, std::variant<uint32, uint16, uint8>>;
	auto runTest = [&](test_data_t data) {
		auto [immS32, regMemValue, signExtend, switchEndian, memoryValue] = data;
		uint64 memoryIndex = immS32 + regMemValue;
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regD64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regD32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regMem64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regMem32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				uint32 copyWidth;
				if (std::holds_alternative<uint32>(memoryValue))
				{
					copyWidth = 32;
					setMemoryValue(memoryIndex, std::get<uint32>(memoryValue));
				}
				else if (std::holds_alternative<uint16>(memoryValue))
				{
					copyWidth = 16;
					setMemoryValue(memoryIndex, std::get<uint16>(memoryValue));
				}
				else
				{
					copyWidth = 8;
					setMemoryValue(memoryIndex, std::get<uint8>(memoryValue));
				}
				hCPU.gpr[0] = regMemValue;
				emitInst().make_r_name(regMem64, PPCREC_NAME_R0);
				emitInst().make_r_memory(regD32, regMem32, immS32, copyWidth, signExtend, switchEndian);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regD64);
				emitInst().make_name_r(PPCREC_NAME_R0, regMem64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] == regMemValue);
				uint32 actualValue = hCPU.gpr[1];
				if (std::holds_alternative<uint32>(memoryValue))
					assertLoadMemValue(std::get<uint32>(memoryValue), signExtend, switchEndian, actualValue);
				else if (std::holds_alternative<uint16>(memoryValue))
					assertLoadMemValue(std::get<uint16>(memoryValue), signExtend, switchEndian, actualValue);
				else
					assertLoadMemValue(std::get<uint8>(memoryValue), signExtend, switchEndian, actualValue);
			});
	};

	std::initializer_list<test_case_t> testData = {
		{-10, 30, (uint32)12431},
		{3, 2, (uint32)3241242},
		{3, 2, (uint32)-212213},
		{3, 2, (uint16)4313},
		{3, 2, (uint16)-2423},
		{-2, 8, (uint16)2365},
		{3, 4, (uint8)-110},
		{-3, 8, (uint8)120},
	};

	for (auto&& data : testData)
	{
		auto [immS32, regMemValue, memoryValue] = data;
		runTest({immS32, regMemValue, false, false, memoryValue});
		runTest({immS32, regMemValue, false, true, memoryValue});
		runTest({immS32, regMemValue, true, false, memoryValue});
		runTest({immS32, regMemValue, true, true, memoryValue});
	}
}

void load_indexed_tests(setup_and_verify_fn setupAndVerify)
{
	using test_data_t = std::tuple<sint32, uint32, uint32, bool, bool, std::variant<uint32, uint16, uint8>>;
	using test_case_t = std::tuple<sint32, uint32, uint32, std::variant<uint32, uint16, uint8>>;
	auto runTest = [&](test_data_t data) {
		auto [immS32, regMemValue, regMem2Value, signExtend, switchEndian, memoryValue] = data;
		uint64 memoryIndex = immS32 + regMemValue + regMem2Value;
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regD64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regD32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regMem64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regMem32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regMem64_2 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regMem32_2 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);

				uint32 copyWidth;
				if (std::holds_alternative<uint32>(memoryValue))
				{
					copyWidth = 32;
					setMemoryValue(memoryIndex, std::get<uint32>(memoryValue));
				}
				else if (std::holds_alternative<uint16>(memoryValue))
				{
					copyWidth = 16;
					setMemoryValue(memoryIndex, std::get<uint16>(memoryValue));
				}
				else
				{
					copyWidth = 8;
					setMemoryValue(memoryIndex, std::get<uint8>(memoryValue));
				}
				hCPU.gpr[0] = regMemValue;
				hCPU.gpr[1] = regMem2Value;
				emitInst().make_r_name(regMem64, PPCREC_NAME_R0);
				emitInst().make_r_name(regMem64_2, PPCREC_NAME_R0 + 1);
				auto& imlInstruction = emitInst();
				imlInstruction.type = PPCREC_IML_TYPE_LOAD_INDEXED;
				imlInstruction.operation = 0;
				imlInstruction.op_storeLoad.registerData = regD32;
				imlInstruction.op_storeLoad.registerMem = regMem32;
				imlInstruction.op_storeLoad.registerMem2 = regMem32_2;
				imlInstruction.op_storeLoad.immS32 = immS32;
				imlInstruction.op_storeLoad.copyWidth = copyWidth;
				imlInstruction.op_storeLoad.flags2.swapEndian = switchEndian;
				imlInstruction.op_storeLoad.flags2.signExtend = signExtend;
				emitInst().make_name_r(PPCREC_NAME_R0, regMem64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regMem64_2);
				emitInst().make_name_r(PPCREC_NAME_R0 + 2, regD64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] = regMemValue);
				cemu_assert_debug(hCPU.gpr[1] = regMem2Value);
				uint32 actualValue = hCPU.gpr[2];
				if (std::holds_alternative<uint32>(memoryValue))
					assertLoadMemValue(std::get<uint32>(memoryValue), signExtend, switchEndian, actualValue);
				else if (std::holds_alternative<uint16>(memoryValue))
					assertLoadMemValue(std::get<uint16>(memoryValue), signExtend, switchEndian, actualValue);
				else
					assertLoadMemValue(std::get<uint8>(memoryValue), signExtend, switchEndian, actualValue);
			});
	};

	std::initializer_list<test_case_t> testData = {
		{3, 2, 1, (uint32)3241242},
		{3, 2, 3, (uint32)-212213},
		{-3, 2, 3, (uint32)-212213},
		{3, 2, 4, (uint16)4313},
		{-3, 2, 5, (uint16)-2423},
		{3, 8, 5, (uint16)2365},
		{3, 8, 9, (uint8)120},
		{-3, 4, 8, (uint8)-110},
	};

	for (auto&& data : testData)
	{
		auto [immS32, regMemValue, regMem2Value, memoryValue] = data;
		runTest({immS32, regMemValue, regMem2Value, false, false, memoryValue});
		runTest({immS32, regMemValue, regMem2Value, false, true, memoryValue});
		runTest({immS32, regMemValue, regMem2Value, true, false, memoryValue});
		runTest({immS32, regMemValue, regMem2Value, true, true, memoryValue});
	}
}

template<typename T>
	requires is_mem_value<T>
void assertStoreMemValue(T value, bool switchEndian, uint32 memoryIndex)
{
	T actualValue = getMemoryValue<T>(memoryIndex);
	T expectedValue = value;
	if (switchEndian)
		expectedValue = SwapEndian<T>(value);
	cemu_assert_debug(actualValue == expectedValue);
}

void store_tests(setup_and_verify_fn setupAndVerify)
{
	using test_data_t = std::tuple<sint32, uint32, bool, std::variant<uint32, uint16, uint8>>;
	using test_case_t = std::tuple<sint32, uint32, std::variant<uint32, uint16, uint8>>;
	auto runTest = [&](test_data_t data) {
		auto [immS32, regMemValue, switchEndian, memoryValue] = data;
		uint32 memoryIndex = immS32 + regMemValue;
		uint32 copyWidth;
		uint32 sourceMemoryValue;
		if (std::holds_alternative<uint32>(memoryValue))
		{
			copyWidth = 32;
			sourceMemoryValue = std::get<uint32>(memoryValue);
		}
		else if (std::holds_alternative<uint16>(memoryValue))
		{
			copyWidth = 16;
			sourceMemoryValue = std::get<uint16>(memoryValue);
		}
		else
		{
			copyWidth = 8;
			sourceMemoryValue = std::get<uint8>(memoryValue);
		}
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regS64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regS32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regMem64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regMem32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				hCPU.gpr[0] = regMemValue;
				hCPU.gpr[1] = sourceMemoryValue;
				emitInst().make_r_name(regMem64, PPCREC_NAME_R0);
				emitInst().make_r_name(regS64, PPCREC_NAME_R0 + 1);
				emitInst().make_memory_r(regS32, regMem32, immS32, copyWidth, switchEndian);
				emitInst().make_name_r(PPCREC_NAME_R0, regMem64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regS64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] == regMemValue);
				cemu_assert_debug(hCPU.gpr[1] == sourceMemoryValue);
				if (std::holds_alternative<uint32>(memoryValue))
					assertStoreMemValue(std::get<uint32>(memoryValue), switchEndian, memoryIndex);
				else if (std::holds_alternative<uint16>(memoryValue))
					assertStoreMemValue(std::get<uint16>(memoryValue), switchEndian, memoryIndex);
				else
					assertStoreMemValue(std::get<uint8>(memoryValue), switchEndian, memoryIndex);
				assertUninitializedMemory(memoryIndex, copyWidth / 8);
			});
	};

	std::initializer_list<test_case_t> testData = {
		{-4, 5, (uint32)3245242},
		{2, 3, (uint32)3241242},
		{2, 3, (uint16)4313},
		{8, 3, (uint16)2365},
		{8, 3, (uint8)120},
		{-3, 8, (uint8)154},
	};

	for (auto&& data : testData)
	{
		auto [immS32, regMemValue, memoryValue] = data;
		runTest({immS32, regMemValue, false, memoryValue});
		runTest({immS32, regMemValue, true, memoryValue});
	}
}

void store_indexed_tests(setup_and_verify_fn setupAndVerify)
{
	using test_data_t = std::tuple<sint32, uint32, uint32, bool, std::variant<uint32, uint16, uint8>>;
	using test_case_t = std::tuple<sint32, uint32, uint32, std::variant<uint32, uint16, uint8>>;
	auto runTest = [&](test_data_t data) {
		auto [immS32, regMemValue, regMem2Value, switchEndian, memoryValue] = data;
		uint32 copyWidth;
		uint32 sourceMemoryValue;
		if (std::holds_alternative<uint32>(memoryValue))
		{
			copyWidth = 32;
			sourceMemoryValue = std::get<uint32>(memoryValue);
		}
		else if (std::holds_alternative<uint16>(memoryValue))
		{
			copyWidth = 16;
			sourceMemoryValue = std::get<uint16>(memoryValue);
		}
		else
		{
			copyWidth = 8;
			sourceMemoryValue = std::get<uint8>(memoryValue);
		}
		uint32 memoryIndex = immS32 + regMemValue + regMem2Value;
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regS64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regS32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regMem64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regMem32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regMem2_64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regMem2_32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);

				hCPU.gpr[0] = regMemValue;
				hCPU.gpr[1] = regMem2Value;
				hCPU.gpr[2] = sourceMemoryValue;
				emitInst().make_r_name(regMem64, PPCREC_NAME_R0);
				emitInst().make_r_name(regMem2_64, PPCREC_NAME_R0 + 1);
				emitInst().make_r_name(regS64, PPCREC_NAME_R0 + 2);
				auto& imlInstruction = emitInst();
				imlInstruction.type = PPCREC_IML_TYPE_STORE_INDEXED;
				imlInstruction.operation = 0;
				imlInstruction.op_storeLoad.immS32 = immS32;
				imlInstruction.op_storeLoad.registerData = regS32;
				imlInstruction.op_storeLoad.registerMem = regMem32;
				imlInstruction.op_storeLoad.registerMem2 = regMem2_32;
				imlInstruction.op_storeLoad.copyWidth = copyWidth;
				imlInstruction.op_storeLoad.flags2.swapEndian = switchEndian;
				imlInstruction.op_storeLoad.flags2.signExtend = false;
				emitInst().make_name_r(PPCREC_NAME_R0 + 0, regMem64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regMem2_64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 2, regS64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[0] == regMemValue);
				cemu_assert_debug(hCPU.gpr[1] == regMem2Value);
				cemu_assert_debug(hCPU.gpr[2] == sourceMemoryValue);
				if (std::holds_alternative<uint32>(memoryValue))
					assertStoreMemValue(std::get<uint32>(memoryValue), switchEndian, memoryIndex);
				else if (std::holds_alternative<uint16>(memoryValue))
					assertStoreMemValue(std::get<uint16>(memoryValue), switchEndian, memoryIndex);
				else
					assertStoreMemValue(std::get<uint8>(memoryValue), switchEndian, memoryIndex);
				assertUninitializedMemory(memoryIndex, copyWidth / 8);
			});
	};

	std::initializer_list<test_case_t> testData = {
		{3, 2, 4, (uint32)3241242},
		{-3, 2, 4, (uint32)3245242},
		{3, 2, 3, (uint16)4313},
		{-3, 8, 3, (uint16)2365},
		{3, 8, 3, (uint8)120},
		{-3, 9, 3, (uint8)154},
	};

	for (auto&& data : testData)
	{
		auto [immS32, regMemValue, regMemValue2, memoryValue] = data;
		runTest({immS32, regMemValue, regMemValue2, false, memoryValue});
		runTest({immS32, regMemValue, regMemValue2, true, memoryValue});
	}
}

void atomic_cmp_store_tests(setup_and_verify_fn setupAndVerify)
{
	using setup_data_t = std::tuple<uint32, uint32, uint32, uint32>;

	auto runTest = [&](setup_data_t data) {
		auto [memoryIndex, compareValue, memoryValue, writeValue] = data;
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regEA64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regEA32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regCompareValue64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regCompareValue32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);
				IMLReg regWriteValue64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 3);
				IMLReg regWriteValue32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 3);
				IMLReg regSuccessOutput64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 5);
				IMLReg regSuccessOutput32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 5);
				hCPU.gpr[0] = memoryIndex;
				hCPU.gpr[1] = compareValue;
				hCPU.gpr[2] = writeValue;
				setMemoryValue(memoryIndex, memoryValue);
				emitInst().make_r_name(regEA64, PPCREC_NAME_R0);
				emitInst().make_r_name(regCompareValue64, PPCREC_NAME_R0 + 1);
				emitInst().make_r_name(regWriteValue64, PPCREC_NAME_R0 + 2);
				emitInst().make_atomic_cmp_store(regEA32, regCompareValue32, regWriteValue32, regSuccessOutput32);
				emitInst().make_name_r(PPCREC_NAME_R0 + 0, regSuccessOutput64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 1, regCompareValue64);
				emitInst().make_name_r(PPCREC_NAME_R0 + 2, regWriteValue64);
			},
			[&](PPCInterpreter_t& hCPU) {
				cemu_assert_debug(hCPU.gpr[1] == compareValue);
				cemu_assert_debug(hCPU.gpr[2] == writeValue);
				bool isEqual = memoryValue == compareValue;
				cemu_assert_debug(hCPU.gpr[0] == isEqual);
				auto memValue = getMemoryValue<uint32>(memoryIndex);
				uint32 assertValue = memoryValue == compareValue ? writeValue : memoryValue;
				cemu_assert_debug(getMemoryValue<uint32>(memoryIndex) == assertValue);
				assertUninitializedMemory(memoryIndex, 32 / 4);
			});
	};
	std::initializer_list<setup_data_t> testData = {
		{100, 30, 30, 600},
		{0, 31, 30, 600},
		{4, 30, 30, 200},
		{8, 30, 30, 300},
		{16, 30, 31, 300},
		{4, 30, 30, 200},
		{4, 30, 30, 200},
		{4, 32, 30, 200},
	};
	for (auto&& data : testData)
	{
		runTest(data);
	}
}

union fpr_data_t
{
	uint8 ui8_b[16];
	sint8 si8_b[16];
	uint16 ui16_h[8];
	sint16 si16_h[8];
	float f32_s[4];
	uint32 ui32_s[4];
	double f64_d[2];
	sint64 si64_d[2];
	sint64 ui64_d[2];
};
template<typename T>
void SwapEndianVec(T* vec, size_t size)
{
	for (size_t i = 0; i < size; i++)
		*(vec + i) = SwapEndian<T>(*(vec + i));
}
void fpr_load_tests(setup_and_verify_fn setupAndVerify)
{
	using setup_mem_fn = std::function<void()>;
	struct test_data_fpr_load
	{
		fpr_data_t fprData;
		uint32 memoryRegValue;
		sint32 immS32;
		uint8 mode;
		bool notExpanded;
		std::optional<uint32> memoryIndexRegvalue;
		std::optional<uint32> gqrValue;
	};
	auto contains = [](std::initializer_list<uint8> x, uint8 val) {
		return std::find(x.begin(), x.end(), val) != x.end();
	};
	std::initializer_list<uint8> fpr_64bit_size_ops = {
		PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0,
	};
	std::initializer_list<uint8> fpr_32bit_size_ops = {
		PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1,
		PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1,
		PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0,
	};
	std::initializer_list<uint8> fpr_16bit_size_ops = {
		PPCREC_FPR_LD_MODE_PSQ_S16_PS0,
		PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1,
		PPCREC_FPR_LD_MODE_PSQ_U16_PS0,
		PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1,
	};
	std::initializer_list<uint8> fpr_8bit_size_ops = {
		PPCREC_FPR_LD_MODE_PSQ_S8_PS0,
		PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1,
		PPCREC_FPR_LD_MODE_PSQ_U8_PS0,
		PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1,
	};
	auto runTest = [&](test_data_fpr_load data) {
		bool gqrFloat = false;
		bool gqrLoadPS1 = false;
		uint8 mode = data.mode;
		if (data.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 ||
			data.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0)
		{
			gqrLoadPS1 = data.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1;
			gqrFloat = false;
			auto loadTypeField = (data.gqrValue.value() >> 16) & 0x7;
			if (loadTypeField == 4)
				mode = gqrLoadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U8_PS0;
			else if (loadTypeField == 5)
				mode = gqrLoadPS1 ? PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_U16_PS0;
			else if (loadTypeField == 6)
				mode = gqrLoadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S8_PS0;
			else if (loadTypeField == 7)
				mode = gqrLoadPS1 ? PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_S16_PS0;
			else
			{
				gqrFloat = true;
				mode = gqrLoadPS1 ? PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0;
			}
		}
		fpr_data_t initialVal = {
			.f64_d = {
				45513.421,
				763254.23,
			},
		};
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regData64 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				IMLReg regMem64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regMem32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regMemIndex64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regMemIndex32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);
				IMLReg regWriteValue64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 3);
				IMLReg regWriteValue32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 3);
				IMLReg gqr64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 5);
				IMLReg gqr32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 5);
				IMLReg regSuccessOutput64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 6);
				IMLReg regSuccessOutput32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 6);
				auto temp = data.fprData;
				if (contains(fpr_64bit_size_ops, mode))
					SwapEndianVec(temp.ui64_d, 2);
				else if (contains(fpr_32bit_size_ops, mode))
					SwapEndianVec(temp.ui32_s, 4);
				else if (contains(fpr_16bit_size_ops, mode))
					SwapEndianVec(temp.ui16_h, 8);
				int memoryIndex = data.memoryRegValue + data.immS32 + data.memoryIndexRegvalue.value_or(0);
				std::memcpy(memory_base + memoryIndex, &temp, sizeof(fpr_data_t));
				bool isIndexed = data.memoryIndexRegvalue.has_value();
				bool hasGqr = data.gqrValue.has_value();
				hCPU.gpr[0] = data.memoryRegValue;
				emitInst().make_r_name(regMem64, PPCREC_NAME_R0);
				std::memcpy(hCPU.fpr + 1, &initialVal, sizeof(fpr_data_t));
				emitInst().make_r_name(regData64, PPCREC_NAME_FPR0 + 1);
				if (isIndexed)
				{
					hCPU.gpr[1] = data.memoryIndexRegvalue.value();
					emitInst().make_r_name(regMemIndex64, PPCREC_NAME_R0 + 1);
				}
				if (hasGqr)
				{
					hCPU.gpr[2] = data.gqrValue.value();
					emitInst().make_r_name(gqr64, PPCREC_NAME_R0 + 2);
				}
				auto& imlInstruction = emitInst();
				if (isIndexed)
				{
					imlInstruction.type = PPCREC_IML_TYPE_FPR_LOAD_INDEXED;
					imlInstruction.op_storeLoad.registerMem2 = regMemIndex32;
				}
				else
				{
					imlInstruction.type = PPCREC_IML_TYPE_FPR_LOAD;
				}
				imlInstruction.op_storeLoad.registerData = regData64;
				imlInstruction.op_storeLoad.registerMem = regMem32;
				imlInstruction.op_storeLoad.immS32 = data.immS32;
				imlInstruction.op_storeLoad.mode = data.mode;
				imlInstruction.op_storeLoad.registerGQR = hasGqr ? gqr32 : IMLREG_INVALID;
				imlInstruction.op_storeLoad.flags2.notExpanded = data.notExpanded;
				emitInst().make_name_r(PPCREC_NAME_FPR0, regData64);
			},
			[&](PPCInterpreter_t& hCPU) {
				fpr_data_t output = std::bit_cast<fpr_data_t>(hCPU.fpr[0]);
				double ps0FromInt;
				double ps1FromInt;
				if (mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1)
				{
					ps0FromInt = (double)data.fprData.ui8_b[0];
					ps1FromInt = (double)data.fprData.ui8_b[1];
				}
				if (mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0)
					ps0FromInt = (double)data.fprData.ui8_b[0];
				if (mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1)
				{
					ps0FromInt = (double)data.fprData.ui16_h[0];
					ps1FromInt = (double)data.fprData.ui16_h[1];
				}
				if (mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0)
					ps0FromInt = (double)data.fprData.ui16_h[0];
				if (mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1)
				{
					ps0FromInt = (double)data.fprData.si8_b[0];
					ps1FromInt = (double)data.fprData.si8_b[1];
				}
				if (mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0)
					ps0FromInt = (double)data.fprData.si8_b[0];
				if (mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1)
				{
					ps0FromInt = (double)data.fprData.si16_h[0];
					ps1FromInt = (double)data.fprData.si16_h[1];
				}
				if (mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0)
					ps0FromInt = (double)data.fprData.si16_h[0];

				if ((data.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1 ||
					 data.mode == PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0) &&
					!gqrFloat)
				{
					size_t gqrOffset = data.gqrValue.value() >> 24;
					cemu_assert_debug(gqrOffset < 64);
					if (gqrLoadPS1)
					{
						ps0FromInt *= ppcRecompilerInstanceData->_psq_ld_scale_ps0_ps1[gqrOffset * 2];
						ps1FromInt *= ppcRecompilerInstanceData->_psq_ld_scale_ps0_ps1[gqrOffset * 2 + 1];
					}
					else
					{
						ps0FromInt *= ppcRecompilerInstanceData->_psq_ld_scale_ps0_1[gqrOffset * 2];
					}
				}
				if (mode == PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1)
				{
					if (data.notExpanded)
					{
						cemu_assert_debug(output.f32_s[0] == data.fprData.f32_s[0]);
						cemu_assert_debug(output.ui32_s[1] == 0);
						cemu_assert_debug(output.ui64_d[1] == 0);
					}
					else
					{
						cemu_assert_debug(output.f64_d[0] == (double)data.fprData.f32_s[0]);
						cemu_assert_debug(output.f64_d[1] == (double)data.fprData.f32_s[0]);
					}
				}
				else if (mode == PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0)
				{
					cemu_assert_debug(output.f64_d[0] == data.fprData.f64_d[0]);
					cemu_assert_debug(output.f64_d[1] == initialVal.f64_d[1]);
				}
				else if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1)
				{
					cemu_assert_debug(output.f64_d[0] == (double)data.fprData.f32_s[0]);
					cemu_assert_debug(output.f64_d[1] == (double)data.fprData.f32_s[1]);
				}
				else if (mode == PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0)
				{
					cemu_assert_debug(output.f64_d[0] == (double)data.fprData.f32_s[0]);
					cemu_assert_debug(fp_equal(output.f64_d[1], 1.0));
				}
				else if (mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 ||
						 mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0 ||
						 mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 ||
						 mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0 ||
						 mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0 ||
						 mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 ||
						 mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1 ||
						 mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0)
				{
					bool loadPs1 =
						mode == PPCREC_FPR_LD_MODE_PSQ_U8_PS0_PS1 ||
						mode == PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1 ||
						mode == PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1 ||
						mode == PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1;
					cemu_assert_debug(fp_equal(output.f64_d[0], ps0FromInt));
					if (loadPs1)
						cemu_assert_debug(fp_equal(output.f64_d[1], ps1FromInt));
					else
						cemu_assert_debug(fp_equal(output.f64_d[1], 1.0));
				}
				else
				{
					cemu_assert_suspicious();
				}
			});
	};
	// non indexed single
	runTest(
		{
			.fprData = {
				.f32_s = {
					12321.536f,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 4,
			.mode = PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1,
			.notExpanded = true,
		});
	runTest(
		{
			.fprData = {
				.f32_s = {
					1342.536f,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1,
			.notExpanded = false,
		});
	// indexed single
	runTest(
		{
			.fprData = {
				.f32_s = {
					12321.536f,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 4,
			.mode = PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1,
			.notExpanded = true,
			.memoryIndexRegvalue = 4,
		});
	runTest(
		{
			.fprData = {
				.f32_s = {
					1342.536f,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_SINGLE_INTO_PS0_PS1,
			.notExpanded = false,
			.memoryIndexRegvalue = 4,
		});
	// non indexed double
	runTest(
		{
			.fprData = {
				.f64_d = {
					72452.256,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0,
			.notExpanded = false,
		});
	// indexed double
	runTest(
		{
			.fprData = {
				.f64_d = {
					72452.256,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_DOUBLE_INTO_PS0,
			.notExpanded = false,
			.memoryIndexRegvalue = 8,
		});
	runTest(
		{
			.fprData = {
				.f32_s = {
					134.524f,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.f32_s = {
					244.23f,
					541.22f,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_FLOAT_PS0_PS1,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.si16_h = {
					-4134,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_S16_PS0,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.si16_h = {
					-3452,
					723,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_S16_PS0_PS1,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.ui16_h = {
					54346,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_U16_PS0,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.ui16_h = {
					57443,
					234,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_U16_PS0_PS1,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.si8_b = {
					-123,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.si8_b = {
					-12,
					123,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.ui8_b = {
					234,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0,
			.notExpanded = false,
		});
	runTest(
		{
			.fprData = {
				.ui8_b = {
					234,
					154,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_S8_PS0_PS1,
			.notExpanded = false,
		});
	// generic ps0
	runTest(
		{
			.fprData = {
				.ui8_b = {
					135,
					2,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0,
			.notExpanded = false,
			.gqrValue = (/*uint8*/ 4 << 16) | (/*offset*/ 2 << 24),
		});
	runTest(
		{
			.fprData = {
				.ui16_h = {
					53343,
					12321,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0,
			.notExpanded = false,
			.gqrValue = (/*uint16*/ 5 << 16) | (/*offset*/ 3 << 24),
		});
	runTest(
		{
			.fprData = {
				.si8_b = {
					-120,
					42,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0,
			.notExpanded = false,
			.gqrValue = (/*sint8*/ 6 << 16) | (/*offset*/ 4 << 24),
		});
	runTest(
		{
			.fprData = {
				.si16_h = {
					-13520,
					21321,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0,
			.notExpanded = false,
			.gqrValue = (/*sint16*/ 7 << 16) | (/*offset*/ 5 << 24),
		});
	runTest(
		{
			.fprData = {
				.f32_s = {
					-1240.513,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0,
			.notExpanded = false,
			.gqrValue = (/*float32*/ 1 << 16) | (/*offset*/ 2 << 24),
		});
	// generic ps0-ps1
	runTest(
		{
			.fprData = {
				.ui8_b = {
					135,
					2,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1,
			.notExpanded = false,
			.gqrValue = (/*uint8*/ 4 << 16) | (/*offset*/ 2 << 24),
		});
	runTest(
		{
			.fprData = {
				.ui16_h = {
					53343,
					12321,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1,
			.notExpanded = false,
			.gqrValue = (/*uint16*/ 5 << 16) | (/*offset*/ 3 << 24),
		});
	runTest(
		{
			.fprData = {
				.si8_b = {
					-120,
					42,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1,
			.notExpanded = false,
			.gqrValue = (/*sint8*/ 6 << 16) | (/*offset*/ 4 << 24),
		});
	runTest(
		{
			.fprData = {
				.si16_h = {
					-13520,
					21321,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1,
			.notExpanded = false,
			.gqrValue = (/*sint16*/ 7 << 16) | (/*offset*/ 5 << 24),
		});
	runTest(
		{
			.fprData = {
				.f32_s = {
					-1240.513,
					4652.4134,
				},
			},
			.memoryRegValue = 4,
			.immS32 = 8,
			.mode = PPCREC_FPR_LD_MODE_PSQ_GENERIC_PS0_PS1,
			.notExpanded = false,
			.gqrValue = (/*float32*/ 1 << 16) | (/*offset*/ 4 << 24),
		});
}

union fpr_data_be_t
{
	float sbe[4];
	double dbe[2];
	uint16 hbe[8];
	uint8 bbe[16];
	template<typename T>
		requires(sizeof(T) == sizeof(double))
	T d(size_t index)
	{
		return SwapEndian<T>(std::bit_cast<T, double>(dbe[index]));
	}
	template<typename T>
		requires(sizeof(T) == sizeof(float))
	T s(size_t index)
	{
		return SwapEndian<T>(std::bit_cast<T, float>(sbe[index]));
	};
	template<typename T>
		requires(sizeof(T) == sizeof(uint16))
	T h(size_t index)
	{
		return SwapEndian<uint16>(std::bit_cast<T, uint16>(hbe[index]));
	};
	template<typename T>
		requires(sizeof(T) == sizeof(uint8))
	T b(size_t index)
	{
		return SwapEndian<uint8>(std::bit_cast<T, uint8>(bbe[index]));
	};
};

auto clampS16 = [](auto x) -> sint32 {
	if (x >= std::numeric_limits<sint32>::max() || x <= std::numeric_limits<sint32>::min())
		x = std::numeric_limits<sint32>::min();
	if (x <= -32768)
		return -32768;
	if (x >= 32767)
		return 32767;
	return (sint16)x;
};
auto clampU16 = [](auto x) -> uint16 {
	if (x >= std::numeric_limits<sint32>::max() || x <= std::numeric_limits<sint32>::min())
		x = std::numeric_limits<sint32>::min();
	if (x <= 0)
		return 0;
	if (x >= 0xFFFF)
		return 0xFFFF;
	return (uint16)x;
};
auto clampS8 = [](auto x) -> sint8 {
	if (x >= std::numeric_limits<sint32>::max() || x <= std::numeric_limits<sint32>::min())
		x = std::numeric_limits<sint32>::min();
	if (x <= -128)
		return -128;
	if (x >= 127)
		return 127;
	return (sint8)x;
};
auto clampU8 = [](auto x) -> uint8 {
	if (x <= 0)
		return 0;
	if (x >= 255)
		return 255;
	return (uint8)x;
};

void fpr_store_tests(setup_and_verify_fn setupAndVerify)
{
	struct test_data_fpr_load
	{
		fpr_data_t dataRegValue;
		uint32 memoryRegValue;
		sint32 immS32;
		uint8 mode;
		bool notExpanded;
		std::optional<uint32> memoryIndexRegvalue;
		std::optional<uint32> gqrValue;
	};
	auto runTest = [&](test_data_fpr_load data) {
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regData64 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				IMLReg regMem64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 1);
				IMLReg regMem32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 1);
				IMLReg regMemIndex64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 2);
				IMLReg regMemIndex32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 2);
				IMLReg gqr64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 5);
				IMLReg gqr32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 5);
				std::memcpy(hCPU.fpr, &data.dataRegValue, 16);
				emitInst().make_r_name(regData64, PPCREC_NAME_FPR0);
				hCPU.gpr[0] = data.memoryRegValue;
				bool isIndexed = data.memoryIndexRegvalue.has_value();
				bool hasGqr = data.gqrValue.has_value();
				emitInst().make_r_name(regMem64, PPCREC_NAME_R0);
				if (isIndexed)
				{
					hCPU.gpr[1] = data.memoryIndexRegvalue.value();
					emitInst().make_r_name(regMemIndex64, PPCREC_NAME_R0 + 1);
				}
				if (hasGqr)
				{
					hCPU.gpr[2] = data.gqrValue.value();
					emitInst().make_r_name(gqr64, PPCREC_NAME_R0 + 2);
				}
				auto& imlInstruction = emitInst();
				if (isIndexed)
				{
					imlInstruction.type = PPCREC_IML_TYPE_FPR_STORE_INDEXED;
					imlInstruction.op_storeLoad.registerMem2 = regMemIndex32;
				}
				else
				{
					imlInstruction.type = PPCREC_IML_TYPE_FPR_STORE;
				}
				imlInstruction.op_storeLoad.registerData = regData64;
				imlInstruction.op_storeLoad.registerMem = regMem32;
				imlInstruction.op_storeLoad.immS32 = data.immS32;
				imlInstruction.op_storeLoad.mode = data.mode;
				imlInstruction.op_storeLoad.registerGQR = hasGqr ? gqr32 : IMLREG_INVALID;
				imlInstruction.op_storeLoad.flags2.notExpanded = data.notExpanded;
			},
			[&](PPCInterpreter_t&) {
				uint32 index = data.memoryRegValue + data.memoryIndexRegvalue.value_or(0) + data.immS32;
				auto memData = *reinterpret_cast<fpr_data_be_t*>(memory_base + index);
				size_t storeSize = 0;
				if (data.mode == PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0)
				{
					storeSize = 32;
					if (data.notExpanded)
						cemu_assert_debug(memData.s<float>(0) == data.dataRegValue.f32_s[0]);
					else
						cemu_assert_debug(fp_equal(memData.s<float>(0), (float)data.dataRegValue.f64_d[0]));
				}
				else if (data.mode == PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0)
				{
					storeSize = 64;
					cemu_assert_debug(fp_equal(memData.d<double>(0), data.dataRegValue.f64_d[0]));
				}
				else if (data.mode == PPCREC_FPR_ST_MODE_UI32_FROM_PS0)
				{
					storeSize = 32;
					cemu_assert_debug(memData.s<uint32>(0) == data.dataRegValue.ui32_s[0]);
				}
				else
				{
					uint8 mode = data.mode;
					if (data.mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1 ||
						data.mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0)
					{
						bool storePS1 = data.mode == PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1;
						bool isFloat = false;
						auto storeTypeField = data.gqrValue.value() & 0x7;
						if (storeTypeField == 4)
							mode = storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U8_PS0;
						else if (storeTypeField == 5)
							mode = storePS1 ? PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_U16_PS0;
						else if (storeTypeField == 6)
							mode = storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S8_PS0;
						else if (storeTypeField == 7)
							mode = storePS1 ? PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_S16_PS0;
						else
						{
							isFloat = true;
							mode = storePS1 ? PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1 : PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0;
						}
						if (!isFloat)
						{
							size_t gqrOffset = data.gqrValue.value() >> 8;
							if (storePS1)
							{
								data.dataRegValue.f64_d[0] *= ppcRecompilerInstanceData->_psq_st_scale_ps0_ps1[gqrOffset * 2];
								data.dataRegValue.f64_d[1] *= ppcRecompilerInstanceData->_psq_st_scale_ps0_ps1[gqrOffset * 2 + 1];
							}
							else
							{
								data.dataRegValue.f64_d[0] *= ppcRecompilerInstanceData->_psq_st_scale_ps0_1[gqrOffset * 2];
							}
						}
					}
					else
					{
						mode = data.mode;
					}
					if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1)
					{
						storeSize = 64;
						cemu_assert_debug(fp_equal(memData.s<float>(0), (float)data.dataRegValue.f64_d[0]));
						cemu_assert_debug(fp_equal(memData.s<float>(1), (float)data.dataRegValue.f64_d[1]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0)
					{
						storeSize = 32;
						cemu_assert_debug(fp_equal(memData.s<float>(0), (float)data.dataRegValue.f64_d[0]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0)
					{
						storeSize = 8;
						cemu_assert_debug(memData.b<sint8>(0) == clampS8(data.dataRegValue.f64_d[0]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1)
					{
						storeSize = 16;
						cemu_assert_debug(memData.b<sint8>(0) == clampS8(data.dataRegValue.f64_d[0]));
						cemu_assert_debug(memData.b<sint8>(1) == clampS8(data.dataRegValue.f64_d[1]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0)
					{
						storeSize = 8;
						cemu_assert_debug(memData.b<uint8>(0) == clampU8(data.dataRegValue.f64_d[0]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1)
					{
						storeSize = 16;
						cemu_assert_debug(memData.b<uint8>(0) == clampU8(data.dataRegValue.f64_d[0]));
						cemu_assert_debug(memData.b<uint8>(1) == clampU8(data.dataRegValue.f64_d[1]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0)
					{
						storeSize = 16;
						auto val = memData.h<sint16>(0);
						cemu_assert_debug(memData.h<sint16>(0) == clampS16(data.dataRegValue.f64_d[0]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1)
					{
						storeSize = 32;
						auto val1 = memData.h<sint16>(0);
						auto val2 = memData.h<sint16>(1);
						cemu_assert_debug(memData.h<sint16>(0) == clampS16(data.dataRegValue.f64_d[0]));
						cemu_assert_debug(memData.h<sint16>(1) == clampS16(data.dataRegValue.f64_d[1]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0)
					{
						storeSize = 16;
						cemu_assert_debug(memData.h<uint16>(0) == clampU16(data.dataRegValue.f64_d[0]));
					}
					else if (mode == PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1)
					{
						storeSize = 32;
						cemu_assert_debug(memData.h<uint16>(0) == clampU16(data.dataRegValue.f64_d[0]));
						cemu_assert_debug(memData.h<uint16>(1) == clampU16(data.dataRegValue.f64_d[1]));
					}
				}
				assertUninitializedMemory(index, storeSize / 8);
			});
	};
	// non-indexed double
	runTest({
		.dataRegValue = {
			.f64_d = {
				12321.21,
				513,
			},
		},
		.memoryRegValue = 4,
		.immS32 = 8,
		.mode = PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0,
		.notExpanded = false,
	});
	// indexed double
	runTest({
		.dataRegValue = {
			.f64_d = {
				7654.21,
				345.765,
			},
		},
		.memoryRegValue = 4,
		.immS32 = 8,
		.mode = PPCREC_FPR_ST_MODE_DOUBLE_FROM_PS0,
		.notExpanded = false,
		.memoryIndexRegvalue = 10,
	});
	// non-indexed single
	runTest({
		.dataRegValue = {
			.f64_d = {
				-4214.554,
			},
		},
		.memoryRegValue = 8,
		.immS32 = 8,
		.mode = PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f32_s = {
				-672.13f,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0,
		.notExpanded = true,
	});
	runTest({
		.dataRegValue = {
			.ui32_s = {
				1232122,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_UI32_FROM_PS0,
		.notExpanded = true,
	});
	// indexed single
	runTest({
		.dataRegValue = {
			.f64_d = {
				-4214.554,
			},
		},
		.memoryRegValue = 8,
		.immS32 = 8,
		.mode = PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0,
		.notExpanded = false,
		.memoryIndexRegvalue = 8,
	});
	runTest({
		.dataRegValue = {
			.f32_s = {
				-672.13f,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_SINGLE_FROM_PS0,
		.notExpanded = true,
		.memoryIndexRegvalue = 8,
	});
	runTest({
		.dataRegValue = {
			.ui32_s = {
				1232122,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_UI32_FROM_PS0,
		.notExpanded = true,
		.memoryIndexRegvalue = 8,
	});
	// float32
	runTest({
		.dataRegValue = {
			.f64_d = {
				531.12,
				624.541,
			}},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0_PS1,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				65322.12,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_FLOAT_PS0,
		.notExpanded = false,
	});
	// int16
	runTest({
		.dataRegValue = {
			.f64_d = {
				-5134.12,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_S16_PS0,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				-233.1232,
				512321.141,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				-4294967297.0,
				-4294967297.0,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_S16_PS0_PS1,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				123215.12,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_U16_PS0,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				2325.1232,
				123.141,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_U16_PS0_PS1,
		.notExpanded = false,
	});
	// int8
	runTest({
		.dataRegValue = {
			.f64_d = {
				-23.12,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_S8_PS0,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				-12.1232,
				4444.141,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_S8_PS0_PS1,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				213.12,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_U8_PS0,
		.notExpanded = false,
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				324.1232,
				123.141,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_U8_PS0_PS1,
		.notExpanded = false,
	});
	// generic int8
	runTest({
		.dataRegValue = {
			.f64_d = {
				-23.12,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0,
		.notExpanded = false,
		.gqrValue = (/*uint8*/ 4) | (/*offset*/ 13 << 8),
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				-12.1232,
				120.141,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1,
		.notExpanded = false,
		.gqrValue = (/*uint8*/ 4) | (/*offset*/ 5 << 8),
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				213.12,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0,
		.notExpanded = false,
		.gqrValue = (/*sint8*/ 6) | (/*offset*/ 7 << 8),
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				324.1232,
				123.141,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1,
		.notExpanded = false,
		.gqrValue = (/*sint8*/ 6) | (/*offset*/ 8 << 8),
	});
	// generic int16
	runTest({
		.dataRegValue = {
			.f64_d = {
				-5134.12,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0,
		.notExpanded = false,
		.gqrValue = (/*sint16*/ 7) | (/*offset*/ 13 << 8),
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				-233.1232,
				512321.141,
			},
		},
		.memoryRegValue = 0,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1,
		.notExpanded = false,
		.gqrValue = (/*sint16*/ 7) | (/*offset*/ 8 << 8),
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				123215.12,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0,
		.notExpanded = false,
		.gqrValue = (/*sint16*/ 5) | (/*offset*/ 3 << 8),
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				2325.1232,
				123.141,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1,
		.notExpanded = false,
		.gqrValue = (/*sint16*/ 5) | (/*offset*/ 12 << 8),
	});
	// generic float32
	runTest({
		.dataRegValue = {
			.f64_d = {
				763.532,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0,
		.notExpanded = false,
		.gqrValue = (/*float32*/ 1) | (/*offset*/ 3 << 8),
	});
	runTest({
		.dataRegValue = {
			.f64_d = {
				215.234,
				614.34,
			},
		},
		.memoryRegValue = 16,
		.immS32 = 4,
		.mode = PPCREC_FPR_ST_MODE_PSQ_GENERIC_PS0_PS1,
		.notExpanded = false,
		.gqrValue = (/*float32*/ 1) | (/*offset*/ 12 << 8),
	});
}

void fpr_r_r_tests(setup_and_verify_fn setupAndVerify)
{
	double temp = frsqrte_espresso(-0.0);
	struct test_data
	{
		fpr_data_t dataRegResult;
		fpr_data_t dataRegOperand;
		fpr_data_t output;
	};
	using verify_data_fn = std::function<void(test_data)>;
	auto runTest = [&](uint8 operation, verify_data_fn verifyData, std::optional<fpr_data_t> dataRegOperandValue = {}) {
		fpr_data_t dataRegOperand = dataRegOperandValue.value_or<fpr_data_t>({
			.f64_d = {
				238787.3,
				8322.6,
			},
		});
		fpr_data_t dataRegResult = {
			.f64_d = {
				5421.9,
				989.3,
			},
		};
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regResult = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				IMLReg regOperand = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 1);
				std::memcpy(&hCPU.fpr[0], &dataRegResult, sizeof(fpr_data_t));
				std::memcpy(&hCPU.fpr[1], &dataRegOperand, sizeof(fpr_data_t));
				emitInst().make_r_name(regResult, PPCREC_NAME_FPR0);
				emitInst().make_r_name(regOperand, PPCREC_NAME_FPR0 + 1);
				auto& imlInstruction = emitInst();
				imlInstruction.type = PPCREC_IML_TYPE_FPR_R_R;
				imlInstruction.operation = operation;
				imlInstruction.op_fpr_r_r.regR = regResult;
				imlInstruction.op_fpr_r_r.regA = regOperand;
				emitInst().make_name_r(PPCREC_NAME_FPR0 + 2, regResult);
			},
			[&](PPCInterpreter_t& hCPU) {
				fpr_data_t output;
				std::memcpy(&output, &hCPU.fpr[2], sizeof(fpr_data_t));
				verifyData({
					.dataRegResult = dataRegResult,
					.dataRegOperand = dataRegOperand,
					.output = output,
				});
			});
	};
	//	runTest(
	//		PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP,
	//		[](test_data data) {
	//			cemu_assert_debug(fp_equal(data.output.f64_d[0], fres_espresso(data.dataRegOperand.f64_d[0])));
	//			cemu_assert_debug(fp_equal(data.output.f64_d[1], fres_espresso(data.dataRegOperand.f64_d[0])));
	//		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM_AND_TOP,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand.f64_d[0]);
		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM_AND_TOP,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand.f64_d[1]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_BOTTOM_TO_TOP,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegResult.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand.f64_d[0]);
		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_BOTTOM_AND_TOP_SWAPPED,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand.f64_d[1]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand.f64_d[0]);
		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_TOP_TO_TOP,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegResult.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand.f64_d[1]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_COPY_TOP_TO_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand.f64_d[1]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegOperand.f64_d[0] * data.dataRegResult.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_MULTIPLY_PAIR,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegOperand.f64_d[0] * data.dataRegResult.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == (data.dataRegOperand.f64_d[1] * data.dataRegResult.f64_d[1]));
		});
	runTest(
		PPCREC_IML_OP_FPR_DIVIDE_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegResult.f64_d[0] / data.dataRegOperand.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_DIVIDE_PAIR,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegResult.f64_d[0] / data.dataRegOperand.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == (data.dataRegResult.f64_d[1] / data.dataRegOperand.f64_d[1]));
		});
	runTest(
		PPCREC_IML_OP_FPR_ADD_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegResult.f64_d[0] + data.dataRegOperand.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_ADD_PAIR,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegResult.f64_d[0] + data.dataRegOperand.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == (data.dataRegResult.f64_d[1] + data.dataRegOperand.f64_d[1]));
		});
	runTest(
		PPCREC_IML_OP_FPR_SUB_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegResult.f64_d[0] - data.dataRegOperand.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_SUB_PAIR,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (data.dataRegResult.f64_d[0] - data.dataRegOperand.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == (data.dataRegResult.f64_d[1] - data.dataRegOperand.f64_d[1]));
		});
	runTest(
		PPCREC_IML_OP_ASSIGN,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand.f64_d[1]);
		});
	// runTest(
	// 	PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP,
	// 	[](test_data data) {
	// 		cemu_assert_debug(fp_equal(data.output.f64_d[0], fres_espresso(data.dataRegOperand.f64_d[0])));
	// 		cemu_assert_debug(fp_equal(data.output.f64_d[1], fres_espresso(data.dataRegOperand.f64_d[0])));
	// 	});
	// runTest(
	// 	PPCREC_IML_OP_FPR_BOTTOM_FRES_TO_BOTTOM_AND_TOP,
	// 	[](test_data data) {
	// 		cemu_assert_debug(fp_equal(data.output.f64_d[0], fres_espresso(data.dataRegOperand.f64_d[0])));
	// 		cemu_assert_debug(fp_equal(data.output.f64_d[1], fres_espresso(data.dataRegOperand.f64_d[0])));
	// 	});
	// runTest(
	// 	PPCREC_IML_OP_FPR_BOTTOM_RECIPROCAL_SQRT,
	// 	[](test_data data) {
	// 		cemu_assert_debug(fp_equal(data.output.f64_d[0], frsqrte_espresso(data.dataRegOperand.f64_d[0])));
	// 		cemu_assert_debug(fp_equal(data.output.f64_d[1], data.dataRegResult.f64_d[1]));
	// 	});
	runTest(
		PPCREC_IML_OP_FPR_NEGATE_PAIR,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == -data.dataRegOperand.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == -data.dataRegOperand.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_ABS_PAIR,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == std::abs(data.dataRegOperand.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == std::abs(data.dataRegOperand.f64_d[1]));
		},
		fpr_data_t{
			.f64_d = {
				-5134.2141,
				1451.23,
			},
		});
	runTest(
		PPCREC_IML_OP_FPR_BOTTOM_FCTIWZ,
		[](test_data data) {
			cemu_assert_debug(data.output.si64_d[0] == (sint64)data.dataRegOperand.f64_d[0]);
			// cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
}

void fpr_r_r_r_tests(setup_and_verify_fn setupAndVerify)
{
	struct test_data
	{
		fpr_data_t dataRegResult;
		fpr_data_t dataRegOperand1;
		fpr_data_t dataRegOperand2;
		fpr_data_t output;
	};
	using verify_data_fn = std::function<void(test_data)>;
	auto runTest = [&](uint8 operation, verify_data_fn verifyData) {
		fpr_data_t dataRegOperand1 = {
			.f64_d = {
				13.93,
				54642.996,
			},
		};
		fpr_data_t dataRegOperand2 = {
			.f64_d = {
				-456.43,
				2358.63,
			},
		};
		fpr_data_t dataRegResult = {
			.f64_d = {
				541.29,
				6662.31,
			},
		};
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regResult = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				IMLReg regOperand1 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 1);
				IMLReg regOperand2 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 2);
				std::memcpy(hCPU.fpr, &dataRegResult, sizeof(fpr_data_t));
				std::memcpy(hCPU.fpr + 1, &dataRegOperand1, sizeof(fpr_data_t));
				std::memcpy(hCPU.fpr + 2, &dataRegOperand2, sizeof(fpr_data_t));
				emitInst().make_r_name(regResult, PPCREC_NAME_FPR0);
				emitInst().make_r_name(regOperand1, PPCREC_NAME_FPR0 + 1);
				emitInst().make_r_name(regOperand2, PPCREC_NAME_FPR0 + 2);
				auto& imlInstruction = emitInst();
				imlInstruction.type = PPCREC_IML_TYPE_FPR_R_R_R;
				imlInstruction.operation = operation;
				imlInstruction.op_fpr_r_r_r.regR = regResult;
				imlInstruction.op_fpr_r_r_r.regA = regOperand1;
				imlInstruction.op_fpr_r_r_r.regB = regOperand2;
				emitInst().make_name_r(PPCREC_NAME_FPR0 + 3, regResult);
			},

			[&](PPCInterpreter_t& hCPU) {
				fpr_data_t output;
				std::memcpy(&output, hCPU.fpr + 3, sizeof(fpr_data_t));
				verifyData({
					.dataRegResult = dataRegResult,
					.dataRegOperand1 = dataRegOperand1,
					.dataRegOperand2 = dataRegOperand2,
					.output = output,
				});
			});
	};
	runTest(
		PPCREC_IML_OP_FPR_MULTIPLY_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(fp_equal(data.output.f64_d[0], data.dataRegOperand1.f64_d[0] * data.dataRegOperand2.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_ADD_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(fp_equal(data.output.f64_d[0], data.dataRegOperand1.f64_d[0] + data.dataRegOperand2.f64_d[0]));
			// TODO: check this?
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand1.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_SUB_PAIR,
		[](test_data data) {
			cemu_assert_debug(fp_equal(data.output.f64_d[0], data.dataRegOperand1.f64_d[0] - data.dataRegOperand2.f64_d[0]));
			cemu_assert_debug(fp_equal(data.output.f64_d[1], data.dataRegOperand1.f64_d[1] - data.dataRegOperand2.f64_d[1]));
		});
	runTest(
		PPCREC_IML_OP_FPR_ADD_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(fp_equal(data.output.f64_d[0], data.dataRegOperand2.f64_d[0] + data.dataRegOperand1.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand1.f64_d[1]);
		});
}

void fpr_r_r_r_r_tests(setup_and_verify_fn setupAndVerify)
{
	struct test_data
	{
		fpr_data_t dataRegResult;
		fpr_data_t dataRegOperand1;
		fpr_data_t dataRegOperand2;
		fpr_data_t dataRegOperand3;
		fpr_data_t output;
	};
	using verify_data_fn = std::function<void(test_data)>;
	auto runTest = [&](uint8 operation, verify_data_fn verifyData) {
		fpr_data_t dataRegOperand1 = {
			.f64_d = {
				-735.213,
				54642.996,
			},
		};
		fpr_data_t dataRegOperand2 = {
			.f64_d = {
				10.53,
				2358.63,
			},
		};
		fpr_data_t dataRegOperand3 = {
			.f64_d = {
				-456.43,
				8654.23,
			},
		};
		fpr_data_t dataRegResult = {
			.f64_d = {
				541.29,
				6662.31,
			},
		};
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regResult = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				IMLReg regOperand1 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 1);
				IMLReg regOperand2 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 2);
				IMLReg regOperand3 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 3);
				std::memcpy(hCPU.fpr, &dataRegResult, sizeof(fpr_data_t));
				std::memcpy(hCPU.fpr + 1, &dataRegOperand1, sizeof(fpr_data_t));
				std::memcpy(hCPU.fpr + 2, &dataRegOperand2, sizeof(fpr_data_t));
				std::memcpy(hCPU.fpr + 3, &dataRegOperand3, sizeof(fpr_data_t));
				emitInst().make_r_name(regResult, PPCREC_NAME_FPR0);
				emitInst().make_r_name(regOperand1, PPCREC_NAME_FPR0 + 1);
				emitInst().make_r_name(regOperand2, PPCREC_NAME_FPR0 + 2);
				emitInst().make_r_name(regOperand3, PPCREC_NAME_FPR0 + 3);
				auto& imlInstruction = emitInst();
				imlInstruction.type = PPCREC_IML_TYPE_FPR_R_R_R_R;
				imlInstruction.operation = operation;
				imlInstruction.op_fpr_r_r_r_r.regR = regResult;
				imlInstruction.op_fpr_r_r_r_r.regA = regOperand1;
				imlInstruction.op_fpr_r_r_r_r.regB = regOperand2;
				imlInstruction.op_fpr_r_r_r_r.regC = regOperand3;
				emitInst().make_name_r(PPCREC_NAME_FPR0 + 4, regResult);
			},

			[&](PPCInterpreter_t& hCPU) {
				fpr_data_t output;
				std::memcpy(&output, hCPU.fpr + 4, sizeof(fpr_data_t));
				verifyData({
					.dataRegResult = dataRegResult,
					.dataRegOperand1 = dataRegOperand1,
					.dataRegOperand2 = dataRegOperand2,
					.dataRegOperand3 = dataRegOperand3,
					.output = output,
				});
			});
	};
	runTest(
		PPCREC_IML_OP_FPR_SUM0,
		[](test_data data) {
			cemu_assert_debug(fp_equal(data.output.f64_d[0], data.dataRegOperand1.f64_d[0] + data.dataRegOperand2.f64_d[1]));
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand3.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_SUM1,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand3.f64_d[0]);
			cemu_assert_debug(fp_equal(data.output.f64_d[1], data.dataRegOperand1.f64_d[0] + data.dataRegOperand2.f64_d[1]));
		});
	runTest(
		PPCREC_IML_OP_FPR_SELECT_BOTTOM,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand2.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegResult.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_SELECT_PAIR,
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == data.dataRegOperand2.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.dataRegOperand3.f64_d[1]);
		});
}

void fpr_r_tests(setup_and_verify_fn setupAndVerify)
{
	struct test_data
	{
		fpr_data_t regRValue;
		fpr_data_t output;
	};
	using verify_data_fn = std::function<void(test_data)>;
	auto runTest = [&](uint8 operation, fpr_data_t regRValue, verify_data_fn verifyData) {
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regResult = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				IMLReg regOperand1 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 1);
				IMLReg regOperand2 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 2);
				IMLReg regOperand3 = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 3);
				std::memcpy(hCPU.fpr, &regRValue, sizeof(fpr_data_t));
				emitInst().make_r_name(regResult, PPCREC_NAME_FPR0);
				auto& imlInstruction = emitInst();
				imlInstruction.type = PPCREC_IML_TYPE_FPR_R;
				imlInstruction.operation = operation;
				imlInstruction.op_fpr_r.regR = regResult;
				emitInst().make_name_r(PPCREC_NAME_FPR0 + 2, regResult);
			},

			[&](PPCInterpreter_t& hCPU) {
				fpr_data_t output;
				std::memcpy(&output, hCPU.fpr + 2, sizeof(fpr_data_t));
				verifyData({
					.regRValue = regRValue,
					.output = output,
				});
			});
	};
	runTest(
		PPCREC_IML_OP_FPR_NEGATE_BOTTOM,
		{
			.f64_d = {
				321.5134,
				21354.213,
			},
		},
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == -data.regRValue.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.regRValue.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_ABS_BOTTOM,
		{
			.f64_d = {
				-541.5134,
				214.213,
			},
		},
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == std::abs(data.regRValue.f64_d[0]));
			cemu_assert_debug(data.output.f64_d[1] == data.regRValue.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_BOTTOM,
		{
			.f64_d = {
				541.5134,
				214.213,
			},
		},
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (double)(float)data.regRValue.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == data.regRValue.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_ROUND_TO_SINGLE_PRECISION_PAIR,
		{
			.f64_d = {
				42124.5134124214,
				245164.2131247,
			},
		},
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (double)(float)data.regRValue.f64_d[0]);
			cemu_assert_debug(data.output.f64_d[1] == (double)(float)data.regRValue.f64_d[1]);
		});
	runTest(
		PPCREC_IML_OP_FPR_EXPAND_BOTTOM32_TO_BOTTOM64_AND_TOP64,
		{
			.f32_s = {
				1513.264f,
				-523.23f,
			},
		},
		[](test_data data) {
			cemu_assert_debug(data.output.f64_d[0] == (double)data.regRValue.f32_s[0]);
			cemu_assert_debug(data.output.f64_d[1] == (double)data.regRValue.f32_s[0]);
		});
}

void fpr_compare_tests(setup_and_verify_fn setupAndVerify)
{
	struct test_data
	{
		fpr_data_t regAValue;
		fpr_data_t regBValue;
	};
	struct assert_data
	{
		fpr_data_t regAValue;
		fpr_data_t regBValue;
		uint32 output;
	};
	using verify_data_fn = std::function<void(assert_data)>;
	auto runTest = [&](IMLCondition cond, test_data data, verify_data_fn verifyData) {
		setupAndVerify(
			[&](emit_inst_fn emitInst, PPCInterpreter_t& hCPU) {
				IMLReg regResult64 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I64, 0, 0);
				IMLReg regResult32 = IMLReg(IMLRegFormat::I64, IMLRegFormat::I32, 0, 0);
				IMLReg regA = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex);
				IMLReg regB = IMLReg(IMLRegFormat::F64, IMLRegFormat::F64, 0, floatRegStartIndex + 1);
				std::memcpy(hCPU.fpr, &data.regAValue, sizeof(fpr_data_t));
				std::memcpy(hCPU.fpr + 1, &data.regBValue, sizeof(fpr_data_t));
				emitInst().make_r_name(regA, PPCREC_NAME_FPR0);
				emitInst().make_r_name(regB, PPCREC_NAME_FPR0 + 1);
				auto& imlInstruction = emitInst();
				imlInstruction.type = PPCREC_IML_TYPE_FPR_COMPARE;
				imlInstruction.op_fpr_compare.regR = regResult32;
				imlInstruction.op_fpr_compare.regA = regA;
				imlInstruction.op_fpr_compare.regB = regB;
				imlInstruction.op_fpr_compare.cond = cond;
				emitInst().make_name_r(PPCREC_NAME_R0, regResult64);
			},
			[&](PPCInterpreter_t& hCPU) {
				verifyData({
					.regAValue = data.regAValue,
					.regBValue = data.regBValue,
					.output = hCPU.gpr[0],
				});
			});
	};
	std::initializer_list<std::tuple<IMLCondition, double, double, bool>> testData = {
		{IMLCondition::UNORDERED_LT, NAN, NAN, false},
		{IMLCondition::UNORDERED_LT, NAN, 124.531, false},
		{IMLCondition::UNORDERED_LT, 124.531, NAN, false},
		{IMLCondition::UNORDERED_LT, 124.531, -24.53, false},
		{IMLCondition::UNORDERED_LT, -24.53, 124.531, true},
		{IMLCondition::UNORDERED_LT, 124.531, 124.531, false},

		{IMLCondition::UNORDERED_GT, NAN, NAN, false},
		{IMLCondition::UNORDERED_GT, NAN, 124.531, false},
		{IMLCondition::UNORDERED_GT, 124.531, NAN, false},
		{IMLCondition::UNORDERED_GT, 124.531, -24.53, true},
		{IMLCondition::UNORDERED_GT, -24.53, 124.531, false},
		{IMLCondition::UNORDERED_GT, 124.531, 124.531, false},

		{IMLCondition::UNORDERED_U, NAN, NAN, true},
		{IMLCondition::UNORDERED_U, NAN, 124.531, true},
		{IMLCondition::UNORDERED_U, 124.531, NAN, true},
		{IMLCondition::UNORDERED_U, 124.531, -24.53, false},
		{IMLCondition::UNORDERED_U, -24.53, 124.531, false},
		{IMLCondition::UNORDERED_U, 124.531, 124.531, false},

		{IMLCondition::UNORDERED_EQ, NAN, NAN, false},
		{IMLCondition::UNORDERED_EQ, NAN, 124.531, false},
		{IMLCondition::UNORDERED_EQ, 124.531, NAN, false},
		{IMLCondition::UNORDERED_EQ, 124.531, -24.53, false},
		{IMLCondition::UNORDERED_EQ, -24.53, 124.531, false},
		{IMLCondition::UNORDERED_EQ, 124.531, 124.531, true},
	};

	for (auto&& data : testData)
	{
		auto [cond, regA, regB, expectedResult] = data;
		runTest(
			cond,
			{
				.regAValue = {
					.f64_d = {
						regA,
					},
				},
				.regBValue = {
					.f64_d = {
						regB,
					},
				},
			},
			[&](assert_data data) {
				cemu_assert_debug(data.output == expectedResult);
			});
	}
}

void runRecompilerTests()
{
	memory_base = new uint8[test_memory_base_size];
	PPCRecompiler_init();
	auto tests = {
		r_name_tests,
		name_r_tests,
		r_r_tests,
		r_s32_tests,
		conditional_r_s32_tests,
		r_r_s32_tests,
		r_r_s32_carry_tests,
		r_r_r_tests,
		r_r_r_carry_tests,
		compare_and_compare_s32_tests,
		load_tests,
		load_indexed_tests,
		store_tests,
		store_indexed_tests,
		atomic_cmp_store_tests,
		fpr_load_tests,
		fpr_store_tests,
		fpr_r_r_tests,
		fpr_r_r_r_tests,
		fpr_r_r_r_r_tests,
		fpr_r_tests,
		fpr_compare_tests,
	};
	setup_and_verify_fn setupAndVerifiyFn = [](setup_fn setupFn, verify_fn verifyFn) {
		ppcImlGenContext_t ppcImlGenContext = {};
		PPCInterpreter_t hCPU;
		std::fill(memory_base, memory_base + test_memory_base_size, 0);
		ppcImlGenContext.currentOutputSegment = ppcImlGenContext.NewSegment();
		emit_inst emitInst = emit_inst(&ppcImlGenContext);
		setupFn(emitInst, hCPU);
		ppcImlGenContext.emitInst().make_macro(PPCREC_IML_MACRO_LEAVE, ppcImlGenContext.ppcAddressOfCurrentInstruction, 0, 0, IMLREG_INVALID);
		PPCRecFunction_t ppcRecFunc;
#if defined(ARCH_X86_64)
		bool successful = PPCRecompiler_generateX64Code(&ppcRecFunc, &ppcImlGenContext);
#elif defined(__aarch64__)
		auto aarch64CodeCtx = PPCRecompiler_generateAArch64Code(&ppcRecFunc, &ppcImlGenContext);
		bool successful = aarch64CodeCtx != nullptr;
#endif
		cemu_assert_debug(successful);
		if (!successful)
			return;
		PPCRecompiler_enterRecompilerCode((uint64)ppcRecFunc.x86Code, (uint64)&hCPU);
		verifyFn(hCPU);
	};
	for (auto&& test : tests)
	{
		test(setupAndVerifiyFn);
	}
	PPCRecompiler_Shutdown();
	delete[] memory_base;
	memory_base = nullptr;
}
