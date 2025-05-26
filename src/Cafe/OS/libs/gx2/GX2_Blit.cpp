#include "Common/precompiled.h"
#include "GX2_Blit.h"
#include "GX2_Command.h"
#include "GX2_Surface.h"
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/Core/LattePM4.h"
#include "Cafe/OS/common/OSCommon.h"
#include "GX2_Resource.h"

namespace GX2
{
	// sets the depth/stencil clear registers and updates clear values in DepthBuffer struct
	void GX2SetClearDepthStencil(GX2DepthBuffer* depthBuffer, float depthClearValue, uint8 stencilClearValue)
	{
		GX2ReserveCmdSpace(4);
		*(uint32*)&depthBuffer->clearDepth = _swapEndianU32(*(uint32*)&depthClearValue);
		depthBuffer->clearStencil = _swapEndianU32(stencilClearValue);
		Latte::LATTE_DB_STENCIL_CLEAR stencilClearReg;
		stencilClearReg.set_clearValue(stencilClearValue);
		Latte::LATTE_DB_DEPTH_CLEAR depthClearReg;
		depthClearReg.set_clearValue(depthClearValue);
		gx2WriteGather_submit(pm4HeaderType3(IT_SET_CONTEXT_REG, 1 + 2),
			Latte::REGADDR::DB_STENCIL_CLEAR - 0xA000,
			stencilClearReg, depthClearReg);
	}

	// similar to GX2SetClearDepthStencil but only sets depth
	void GX2SetClearDepth(GX2DepthBuffer* depthBuffer, float depthClearValue)
	{
		GX2ReserveCmdSpace(3);
		*(uint32*)&depthBuffer->clearDepth = _swapEndianU32(*(uint32*)&depthClearValue);
		Latte::LATTE_DB_DEPTH_CLEAR depthClearReg;
		depthClearReg.set_clearValue(depthClearValue);
		gx2WriteGather_submit(pm4HeaderType3(IT_SET_CONTEXT_REG, 1 + 1),
			Latte::REGADDR::DB_DEPTH_CLEAR - 0xA000,
			depthClearReg);
	}

	// similar to GX2SetClearDepthStencil but only sets stencil
	void GX2SetClearStencil(GX2DepthBuffer* depthBuffer, uint8 stencilClearValue)
	{
		GX2ReserveCmdSpace(3);
		depthBuffer->clearStencil = _swapEndianU32(stencilClearValue);
		Latte::LATTE_DB_STENCIL_CLEAR stencilClearReg;
		stencilClearReg.set_clearValue(stencilClearValue);
		gx2WriteGather_submit(pm4HeaderType3(IT_SET_CONTEXT_REG, 1 + 1),
			Latte::REGADDR::DB_STENCIL_CLEAR - 0xA000,
			stencilClearReg);
	}

	// update DB_STENCIL_CLEAR and DB_STENCIL_CLEAR based on clear flags
	void _updateDepthStencilClearRegs(float depthClearValue, uint8 stencilClearValue, GX2ClearFlags clearFlags)
	{
		if ((clearFlags & GX2ClearFlags::SET_DEPTH_REG) != 0 && (clearFlags & GX2ClearFlags::SET_STENCIL_REG) != 0)
		{
			GX2ReserveCmdSpace(4);
			Latte::LATTE_DB_STENCIL_CLEAR stencilClearReg;
			stencilClearReg.set_clearValue(stencilClearValue);
			Latte::LATTE_DB_DEPTH_CLEAR depthClearReg;
			depthClearReg.set_clearValue(depthClearValue);
			gx2WriteGather_submit(pm4HeaderType3(IT_SET_CONTEXT_REG, 1 + 2),
				Latte::REGADDR::DB_STENCIL_CLEAR - 0xA000,
				stencilClearReg, depthClearReg);
		}
		else if ((clearFlags & GX2ClearFlags::SET_DEPTH_REG) != 0)
		{
			GX2ReserveCmdSpace(3);
			Latte::LATTE_DB_DEPTH_CLEAR depthClearReg;
			depthClearReg.set_clearValue(depthClearValue);
			gx2WriteGather_submit(pm4HeaderType3(IT_SET_CONTEXT_REG, 1 + 1),
				Latte::REGADDR::DB_DEPTH_CLEAR - 0xA000,
				depthClearReg);
		}
		else if ((clearFlags & GX2ClearFlags::SET_STENCIL_REG) != 0)
		{
			GX2ReserveCmdSpace(3);
			Latte::LATTE_DB_STENCIL_CLEAR stencilClearReg;
			stencilClearReg.set_clearValue(stencilClearValue);
			gx2WriteGather_submit(pm4HeaderType3(IT_SET_CONTEXT_REG, 1 + 1),
				Latte::REGADDR::DB_STENCIL_CLEAR - 0xA000,
				stencilClearReg);
		}
	}

	void SubmitHLEClear(GX2ColorBuffer* colorBuffer, float colorRGBA[4], GX2DepthBuffer* depthBuffer, float depthClearValue, uint8 stencilClearValue, bool clearColor, bool clearDepth, bool clearStencil)
	{
		GX2ReserveCmdSpace(50);
		uint32 hleClearFlags = 0;
		if (clearColor)
			hleClearFlags |= 1;
		if (clearDepth)
			hleClearFlags |= 2;
		if (clearStencil)
			hleClearFlags |= 4;
		// color buffer
		MPTR colorPhysAddr = MPTR_NULL;
		uint32 colorFormat = 0;
		uint32 colorTileMode = 0;
		uint32 colorWidth = 0;
		uint32 colorHeight = 0;
		uint32 colorPitch = 0;
		uint32 colorFirstSlice = 0;
		uint32 colorNumSlices = 0;
		if (colorBuffer != nullptr)
		{
			colorPhysAddr = memory_virtualToPhysical(colorBuffer->surface.imagePtr);
			colorFormat = (uint32)colorBuffer->surface.format.value();
			colorTileMode = (uint32)colorBuffer->surface.tileMode.value();
			colorWidth = colorBuffer->surface.width;
			colorHeight = colorBuffer->surface.height;
			colorPitch = colorBuffer->surface.pitch;
			colorFirstSlice = _swapEndianU32(colorBuffer->viewFirstSlice);
			colorNumSlices = _swapEndianU32(colorBuffer->viewNumSlices);
		}
		// depth buffer
		MPTR depthPhysAddr = MPTR_NULL;
		uint32 depthFormat = 0;
		uint32 depthTileMode = 0;
		uint32 depthWidth = 0;
		uint32 depthHeight = 0;
		uint32 depthPitch = 0;
		uint32 depthFirstSlice = 0;
		uint32 depthNumSlices = 0;
		if (depthBuffer != nullptr)
		{
			depthPhysAddr = memory_virtualToPhysical(depthBuffer->surface.imagePtr);
			depthFormat = (uint32)depthBuffer->surface.format.value();
			depthTileMode = (uint32)depthBuffer->surface.tileMode.value();
			depthWidth = depthBuffer->surface.width;
			depthHeight = depthBuffer->surface.height;
			depthPitch = depthBuffer->surface.pitch;
			depthFirstSlice = _swapEndianU32(depthBuffer->viewFirstSlice);
			depthNumSlices = _swapEndianU32(depthBuffer->viewNumSlices);
		}
		gx2WriteGather_submit(pm4HeaderType3(IT_HLE_CLEAR_COLOR_DEPTH_STENCIL, 23),
		hleClearFlags,
		colorPhysAddr,
		colorFormat,
		colorTileMode,
		colorWidth,
		colorHeight,
		colorPitch,
		colorFirstSlice,
		colorNumSlices,
		depthPhysAddr,
		depthFormat,
		depthTileMode,
		depthWidth,
		depthHeight,
		depthPitch,
		depthFirstSlice,
		depthNumSlices,
		(uint32)(colorRGBA[0] * 255.0f),
		(uint32)(colorRGBA[1] * 255.0f),
		(uint32)(colorRGBA[2] * 255.0f),
		(uint32)(colorRGBA[3] * 255.0f),
		*(uint32*)&depthClearValue,
		stencilClearValue&0xFF);
	}

	void GX2ClearColor(GX2ColorBuffer* colorBuffer, float r, float g, float b, float a)
	{
		if ((colorBuffer->surface.resFlag & GX2_RESFLAG_USAGE_COLOR_BUFFER) != 0)
		{
			float colorRGBA[4] = { r, g, b, a };
			SubmitHLEClear(colorBuffer, colorRGBA, nullptr, 0.0f, 0, true, false, false);
		}
		else
		{
			debug_printf("GX2ClearColor() - unsupported surface flags\n");
		}
	}

	void GX2ClearBuffersEx(GX2ColorBuffer* colorBuffer, GX2DepthBuffer* depthBuffer, float r, float g, float b, float a, float depthClearValue, uint8 stencilClearValue, GX2ClearFlags clearFlags)
	{
		_updateDepthStencilClearRegs(depthClearValue, stencilClearValue, clearFlags);

		uint32 hleClearFlags = 0;
		if ((clearFlags & GX2ClearFlags::CLEAR_DEPTH) != 0)
			hleClearFlags |= 2;
		if ((clearFlags & GX2ClearFlags::CLEAR_STENCIL) != 0)
			hleClearFlags |= 4;
		hleClearFlags |= 1;

		float colorRGBA[4] = { r, g, b, a };
		SubmitHLEClear(colorBuffer, colorRGBA, depthBuffer, depthClearValue, stencilClearValue, true, (clearFlags & GX2ClearFlags::CLEAR_DEPTH) != 0, (clearFlags & GX2ClearFlags::CLEAR_STENCIL) != 0);
	}

	// always uses passed depthClearValue/stencilClearValue for clearing, even if clear flags dont specify value updates
	void GX2ClearDepthStencilEx(GX2DepthBuffer* depthBuffer, float depthClearValue, uint8 stencilClearValue, GX2ClearFlags clearFlags)
	{
		if (!depthBuffer && (depthBuffer->surface.width == 0 || depthBuffer->surface.height == 0))
		{
			// Super Smash Bros tries to clear an uninitialized depth surface?
			debug_printf("GX2ClearDepthStencilEx(): Attempting to clear invalid depthbuffer\n");
			return;
		}

		_updateDepthStencilClearRegs(depthClearValue, stencilClearValue, clearFlags);

		float colorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		SubmitHLEClear(nullptr, colorRGBA, depthBuffer, depthClearValue, stencilClearValue, false, (clearFlags & GX2ClearFlags::CLEAR_DEPTH) != 0, (clearFlags & GX2ClearFlags::CLEAR_STENCIL) != 0);
	}

	void GX2BlitInit()
	{
		cafeExportRegister("gx2", GX2SetClearDepthStencil, LogType::GX2);
		cafeExportRegister("gx2", GX2SetClearDepth, LogType::GX2);
		cafeExportRegister("gx2", GX2SetClearStencil, LogType::GX2);
		
		cafeExportRegister("gx2", GX2ClearColor, LogType::GX2);
		cafeExportRegister("gx2", GX2ClearBuffersEx, LogType::GX2);
		cafeExportRegister("gx2", GX2ClearDepthStencilEx, LogType::GX2);
	}
}