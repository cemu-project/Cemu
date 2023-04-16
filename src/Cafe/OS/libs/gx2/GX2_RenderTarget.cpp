#include "Cafe/OS/common/OSCommon.h"
#include "GX2.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"

#include "Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.h"

#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"

#include "GX2_Command.h"

void gx2Export_GX2InitColorBufferRegs(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2InitColorBufferRegs(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(colorBuffer, GX2ColorBuffer, 0);

	LatteAddrLib::AddrSurfaceInfo_OUT surfaceInfo;
	LatteAddrLib::GX2CalculateSurfaceInfo(colorBuffer->surface.format, colorBuffer->surface.width, colorBuffer->surface.height, colorBuffer->surface.depth, colorBuffer->surface.dim, colorBuffer->surface.tileMode, colorBuffer->surface.aa, _swapEndianU32(colorBuffer->viewMip), &surfaceInfo);

	uint32 pitchHeight = (surfaceInfo.height * surfaceInfo.pitch) >> 6;
#ifdef CEMU_DEBUG_ASSERT
	if (colorBuffer->viewNumSlices != _swapEndianU32(1))
		cemuLog_logDebug(LogType::Force, "GX2InitColorBufferRegs(): With unsupported slice count {}", _swapEndianU32(colorBuffer->viewNumSlices));
	if (surfaceInfo.pitch < 7)
		cemuLog_logDebug(LogType::Force, "GX2InitColorBufferRegs(): Pitch too small (pitch = {})", surfaceInfo.pitch);
	if ((surfaceInfo.pitch & 7) != 0)
		cemuLog_logDebug(LogType::Force, "GX2InitColorBufferRegs(): Pitch has invalid alignment (pitch = {})", surfaceInfo.pitch);
	if (pitchHeight == 0)
		cemuLog_logDebug(LogType::Force, "GX2InitColorBufferRegs(): Invalid value (pitchHeight = {})", pitchHeight);
#endif

	uint32 cSize = ((surfaceInfo.pitch >> 3) - 1) & 0x3FF;
	cSize |= (((pitchHeight - 1) & 0xFFFFF) << 10);
	colorBuffer->reg_size = cSize;
	colorBuffer->reg_mask = 0;
	// reg color_info
	Latte::E_GX2SURFFMT format = colorBuffer->surface.format;
	Latte::E_HWSURFFMT hwFormat = Latte::GetHWFormat(format);
	uint32 formatHighBits = (uint32)format & 0xF00;
	uint32 regInfo = 0;
	regInfo = (uint32)GX2::GetSurfaceFormatSwapMode(colorBuffer->surface.format);
	regInfo |= ((uint32)hwFormat<<2);
	cemu_assert_debug(LatteAddrLib::IsValidHWTileMode(surfaceInfo.hwTileMode));
	regInfo |= ((uint32)surfaceInfo.hwTileMode << 8);
	bool clampBlend = false;
	if (formatHighBits == 0x000)
	{
		regInfo |= (0 << 12);
		clampBlend = true;
	}
	else if (formatHighBits == 0x100) // integer
	{
		regInfo |= (4 << 12);
	}
	else if (formatHighBits == 0x200) // signed
	{
		regInfo |= (1 << 12);
		clampBlend = true;
	}
	else if (formatHighBits == 0x300) // integer + signed
	{
		regInfo |= (5 << 12);
	}
	else if (formatHighBits == 0x400) // srgb
	{
		clampBlend = true;
		regInfo |= (6 << 12);
	}
	else if (formatHighBits == 0x800) // float
	{
		regInfo |= (7 << 12);
	}
	else
		cemu_assert_debug(false);
	if (hwFormat == Latte::E_HWSURFFMT::HWFMT_5_5_5_1 || hwFormat == Latte::E_HWSURFFMT::HWFMT_10_10_10_2 )
		regInfo |= (2 << 16);
	else
		regInfo &= ~(3 << 16); // COMP_SWAP_mask
	if(colorBuffer->surface.aa != 0)
		regInfo |= (2 << 18); // TILE_MODE
	bool isIntegerFormat = (uint32)(format & Latte::E_GX2SURFFMT::FMT_BIT_INT) != 0;
	if (isIntegerFormat == false)
		regInfo |= (GX2::GetSurfaceColorBufferExportFormat(colorBuffer->surface.format) << 27); // 0 -> full, 1 -> normalized
	if (isIntegerFormat
		|| format ==Latte::E_GX2SURFFMT::R24_X8_UNORM
		|| format ==Latte::E_GX2SURFFMT::R24_X8_FLOAT
		|| format ==Latte::E_GX2SURFFMT::R32_X8_FLOAT)
	{
		// set the blend bypass bit for formats which dont support blending
		regInfo |= (1<<22);
		clampBlend = false;
	}
	if (clampBlend)
		regInfo |= (1<<20); // BLEND_CLAMP_bit
	if ((uint32)(format & Latte::E_GX2SURFFMT::FMT_BIT_FLOAT) != 0)
		regInfo |= (1<<25); // ROUND_MODE_bit
	colorBuffer->reg_info = regInfo;
	// reg color_view
	uint32 regView = 0;
	if (colorBuffer->surface.tileMode != Latte::E_GX2TILEMODE::TM_LINEAR_SPECIAL)
	{
		regView |= (_swapEndianU32(colorBuffer->viewFirstSlice) & 0x7FF);
		regView |= (((_swapEndianU32(colorBuffer->viewNumSlices) + _swapEndianU32(colorBuffer->viewFirstSlice) - 1) & 0x7FF) << 13);
	}
	colorBuffer->reg_view = regView;
	colorBuffer->reg_mask = 0;

	// todo - aa stuff

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2InitDepthBufferRegs(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2InitDepthBufferRegs(0x{:08x})", hCPU->gpr[3]);
	ppcDefineParamStructPtr(depthBuffer, GX2DepthBuffer, 0);

	LatteAddrLib::AddrSurfaceInfo_OUT surfaceInfo;
	LatteAddrLib::GX2CalculateSurfaceInfo(depthBuffer->surface.format, depthBuffer->surface.width, depthBuffer->surface.height, depthBuffer->surface.depth, depthBuffer->surface.dim, depthBuffer->surface.tileMode, depthBuffer->surface.aa, _swapEndianU32(depthBuffer->viewMip), &surfaceInfo);

	cemu_assert_debug(depthBuffer->viewNumSlices != 0);

	uint32 cSize = ((surfaceInfo.pitch >> 3) - 1) & 0x3FF;
	cSize |= ((((surfaceInfo.height * surfaceInfo.pitch >> 6) - 1) & 0xFFFFF) << 10);

	depthBuffer->reg_size = cSize;
	// todo - other regs

	osLib_returnFromFunction(hCPU, 0);
}


void gx2Export_GX2SetColorBuffer(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SetColorBuffer(0x{:08x}, {})", hCPU->gpr[3], hCPU->gpr[4]);
	GX2ReserveCmdSpace(20);

	GX2ColorBuffer* colorBufferBE = (GX2ColorBuffer*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);

#ifdef CEMU_DEBUG_ASSERT
	cemuLog_log(LogType::GX2, "ColorBuffer tileMode {:01x} PhysAddr {:08x} fmt {:04x} res {}x{} Mip {} Slice {}", (uint32)colorBufferBE->surface.tileMode.value(), (uint32)colorBufferBE->surface.imagePtr, (uint32)colorBufferBE->surface.format.value(), (uint32)colorBufferBE->surface.width, (uint32)colorBufferBE->surface.height, _swapEndianU32(colorBufferBE->viewMip), _swapEndianU32(colorBufferBE->viewFirstSlice));
#endif

	// regs[0] = mmCB_COLOR0_SIZE
	// regs[1] = mmCB_COLOR0_INFO
	// regs[2] = mmCB_COLOR0_VIEW
	// regs[3] = mmCB_COLOR0_MASK
	// regs[4] = mmCB_COLOR0_TILE

	uint32 targetIndex = hCPU->gpr[4];

	uint32 viewMip = _swapEndianU32(colorBufferBE->viewMip);
	uint32 colorBufferBase = memory_virtualToPhysical(colorBufferBE->surface.imagePtr);

	if( viewMip != 0 )
	{
		uint32 baseImagePtr = colorBufferBE->surface.mipPtr;
		if( viewMip == 1 )
			colorBufferBase = memory_virtualToPhysical(baseImagePtr);
		else
			colorBufferBase = memory_virtualToPhysical(baseImagePtr+colorBufferBE->surface.mipOffset[viewMip-1]);

	}

	Latte::E_GX2TILEMODE tileMode = colorBufferBE->surface.tileMode;
	uint32 viewMipIndex = _swapEndianU32(colorBufferBE->viewMip);
	uint32 swizzle = colorBufferBE->surface.swizzle;

	if (Latte::TM_IsMacroTiled(tileMode) && viewMipIndex < ((swizzle >> 16) & 0xFF))
	{
		// remove swizzle for small mips
		colorBufferBase ^= (swizzle & 0xFFFF);
	}
	// set color buffer pointer for render target
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmCB_COLOR0_BASE - 0xA000 + hCPU->gpr[4]);
	gx2WriteGather_submitU32AsBE(colorBufferBase);
	// set color buffer size
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmCB_COLOR0_SIZE - 0xA000 + hCPU->gpr[4]);
	gx2WriteGather_submitU32AsBE((uint32)colorBufferBE->reg_size);

	cemu_assert_debug(tileMode != Latte::E_GX2TILEMODE::TM_LINEAR_SPECIAL);

	// set mmCB_COLOR*_VIEW
	gx2WriteGather_submit(
		pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
		mmCB_COLOR0_VIEW - 0xA000 + hCPU->gpr[4],
		colorBufferBE->reg_view);

	// todo: mmCB_COLOR0_TILE and mmCB_COLOR0_FRAG

	// set mmCB_COLOR*_INFO
	gx2WriteGather_submit(
		pm4HeaderType3(IT_SET_CONTEXT_REG, 2),
		mmCB_COLOR0_INFO - 0xA000 + hCPU->gpr[4],
		colorBufferBE->reg_info);

	GX2::GX2WriteGather_checkAndInsertWrapAroundMark();

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetDepthBuffer(PPCInterpreter_t* hCPU)
{
	cemuLog_log(LogType::GX2, "GX2SetDepthBuffer(0x{:08x})", hCPU->gpr[3]);
	GX2ReserveCmdSpace(20);

	GX2DepthBuffer* depthBufferBE = (GX2DepthBuffer*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);

	cemuLog_log(LogType::GX2, "DepthBuffer tileMode {:01x} PhysAddr {:08x} fmt {:04x} res {}x{}", (uint32)depthBufferBE->surface.tileMode.value(), (uint32)depthBufferBE->surface.imagePtr, (uint32)depthBufferBE->surface.format.value(), (uint32)depthBufferBE->surface.width, (uint32)depthBufferBE->surface.height);

	uint32 viewMip = _swapEndianU32(depthBufferBE->viewMip);

	// todo: current code for the PM4 packets is a hack, replace with proper implementation

	uint32 regHTileDataBase = memory_virtualToPhysical(depthBufferBE->surface.imagePtr)>>8;

	if( viewMip > 0 )
	{
		cemuLog_logDebug(LogType::Force, "GX2SetDepthBuffer: Unsupported non-zero mip ({}) Pointer: {:08x} Base: {:08x}", viewMip, regHTileDataBase, 0);
	}

	// setup depthbuffer info register
	uint32 regDepthBufferInfo = 0;
	uint32 depthBufferTileMode = (uint32)depthBufferBE->surface.tileMode.value();
	Latte::E_GX2SURFFMT depthBufferFormat = depthBufferBE->surface.format;

	regDepthBufferInfo |= ((depthBufferTileMode&0xF)<<15);
	if (depthBufferFormat == Latte::E_GX2SURFFMT::D16_UNORM)
		regDepthBufferInfo |= (1 << 0);
	else if (depthBufferFormat == Latte::E_GX2SURFFMT::D24_S8_UNORM)
		regDepthBufferInfo |= (3 << 0);
	else if (depthBufferFormat == Latte::E_GX2SURFFMT::D32_FLOAT)
		regDepthBufferInfo |= (6 << 0);
	else if (depthBufferFormat == Latte::E_GX2SURFFMT::D32_S8_FLOAT)
		regDepthBufferInfo |= (7 << 0);
	else if (depthBufferFormat == Latte::E_GX2SURFFMT::D24_S8_FLOAT)
		regDepthBufferInfo |= (5 << 0);
	else
	{
		debug_printf("Unsupported depth buffer format 0x%04x\n", depthBufferFormat);
	}

	// set color buffer pointer for render target
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+1));
	gx2WriteGather_submitU32AsBE(mmDB_DEPTH_SIZE - 0xA000);
	gx2WriteGather_submitU32AsBE((uint32)depthBufferBE->reg_size); // hack
	// set color buffer size
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 1+3));
	gx2WriteGather_submitU32AsBE(mmDB_DEPTH_BASE - 0xA000);

	gx2WriteGather_submitU32AsBE(0); // DB_DEPTH_BASE
	gx2WriteGather_submitU32AsBE(regDepthBufferInfo); // DB_DEPTH_INFO
	gx2WriteGather_submitU32AsBE(regHTileDataBase); // DB_HTILE_DATA_BASE

	// set DB_DEPTH_VIEW
	uint32 db_view = 0;
	db_view |= (_swapEndianU32(depthBufferBE->viewFirstSlice)&0x7FF);
	db_view |= (((_swapEndianU32(depthBufferBE->viewNumSlices)+_swapEndianU32(depthBufferBE->viewFirstSlice)-1)&0x7FF)<<13);
	gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_SET_CONTEXT_REG, 2));
	gx2WriteGather_submitU32AsBE(mmDB_DEPTH_VIEW - 0xA000);
	gx2WriteGather_submitU32AsBE(db_view);

	GX2::GX2WriteGather_checkAndInsertWrapAroundMark();

	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2SetDRCBuffer(PPCInterpreter_t* hCPU)
{
	Latte::E_GX2SURFFMT format = (Latte::E_GX2SURFFMT)hCPU->gpr[6];
	LatteGPUState.drcBufferUsesSRGB = HAS_FLAG(format, Latte::E_GX2SURFFMT::FMT_BIT_SRGB);
	osLib_returnFromFunction(hCPU, 0);
}

void gx2Export_GX2MarkScanBufferCopied(PPCInterpreter_t* hCPU)
{
	uint32 scanTarget = hCPU->gpr[3];
	if( scanTarget == GX2_SCAN_TARGET_TV )
	{
		GX2ReserveCmdSpace(10);

		uint32 physAddr = (MEMORY_TILINGAPERTURE_AREA_ADDR+0x200000);

		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_COPY_COLORBUFFER_TO_SCANBUFFER, 9));
		gx2WriteGather_submitU32AsBE(physAddr);
		gx2WriteGather_submitU32AsBE(1920);
		gx2WriteGather_submitU32AsBE(1080);

		gx2WriteGather_submitU32AsBE(1920); // pitch
		gx2WriteGather_submitU32AsBE(4); // tileMode
		gx2WriteGather_submitU32AsBE(0); // swizzle

		gx2WriteGather_submitU32AsBE(0);
		gx2WriteGather_submitU32AsBE((uint32)Latte::E_GX2SURFFMT::R8_G8_B8_A8_UNORM);
		gx2WriteGather_submitU32AsBE(scanTarget);
	}

	osLib_returnFromFunction(hCPU, 0);
}
