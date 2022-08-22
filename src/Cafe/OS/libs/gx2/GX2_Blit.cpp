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

	void GX2ClearColor(GX2ColorBuffer* colorBuffer, float r, float g, float b, float a)
	{
		GX2ReserveCmdSpace(50);
		if ((colorBuffer->surface.resFlag & GX2_RESFLAG_USAGE_COLOR_BUFFER) != 0)
		{
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_CLEAR_COLOR_DEPTH_STENCIL, 23));
			gx2WriteGather_submitU32AsBE(1); // color (1)
			gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(colorBuffer->surface.imagePtr));
			gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.format.value());
			gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.tileMode.value());
			gx2WriteGather_submitU32AsBE(colorBuffer->surface.width);
			gx2WriteGather_submitU32AsBE(colorBuffer->surface.height);
			gx2WriteGather_submitU32AsBE(colorBuffer->surface.pitch);
			gx2WriteGather_submitU32AsBE(_swapEndianU32(colorBuffer->viewFirstSlice));
			gx2WriteGather_submitU32AsBE(_swapEndianU32(colorBuffer->viewNumSlices));
			gx2WriteGather_submitU32AsBE(MPTR_NULL);
			gx2WriteGather_submitU32AsBE(0); // depth buffer format
			gx2WriteGather_submitU32AsBE(0); // tilemode for depth buffer
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE((uint32)(r * 255.0f));
			gx2WriteGather_submitU32AsBE((uint32)(g * 255.0f));
			gx2WriteGather_submitU32AsBE((uint32)(b * 255.0f));
			gx2WriteGather_submitU32AsBE((uint32)(a * 255.0f));
			gx2WriteGather_submitU32AsBE(0); // clear depth
			gx2WriteGather_submitU32AsBE(0); // clear stencil
		}
		else
		{
			debug_printf("GX2ClearColor() - unsupported surface flags\n");
		}
	}

	void GX2ClearBuffersEx(GX2ColorBuffer* colorBuffer, GX2DepthBuffer* depthBuffer, float r, float g, float b, float a, float depthClearValue, uint8 stencilClearValue, GX2ClearFlags clearFlags)
	{
		GX2ReserveCmdSpace(50);
		_updateDepthStencilClearRegs(depthClearValue, stencilClearValue, clearFlags);

		uint32 hleClearFlags = 0;
		if ((clearFlags & GX2ClearFlags::CLEAR_DEPTH) != 0)
			hleClearFlags |= 2;
		if ((clearFlags & GX2ClearFlags::CLEAR_STENCIL) != 0)
			hleClearFlags |= 4;
		hleClearFlags |= 1;

		// send command to clear color, depth and stencil
		if (_swapEndianU32(colorBuffer->viewFirstSlice) != 0)
			debugBreakpoint();
		gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_CLEAR_COLOR_DEPTH_STENCIL, 23));
		gx2WriteGather_submitU32AsBE(hleClearFlags); // color (1), depth (2), stencil (4)
		gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(colorBuffer->surface.imagePtr));
		gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.format.value());
		gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.tileMode.value());
		gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.width);
		gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.height);
		gx2WriteGather_submitU32AsBE((uint32)colorBuffer->surface.pitch);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(colorBuffer->viewFirstSlice));
		gx2WriteGather_submitU32AsBE(_swapEndianU32(colorBuffer->viewNumSlices));
		gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(depthBuffer->surface.imagePtr));
		gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.format.value());
		gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.tileMode.value());
		gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.width);
		gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.height);
		gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.pitch);
		gx2WriteGather_submitU32AsBE(_swapEndianU32(depthBuffer->viewFirstSlice));
		gx2WriteGather_submitU32AsBE(_swapEndianU32(depthBuffer->viewNumSlices));

		gx2WriteGather_submitU32AsBE((uint32)(r * 255.0f));
		gx2WriteGather_submitU32AsBE((uint32)(g * 255.0f));
		gx2WriteGather_submitU32AsBE((uint32)(b * 255.0f));
		gx2WriteGather_submitU32AsBE((uint32)(a * 255.0f));

		gx2WriteGather_submitU32AsBE(*(uint32*)&depthClearValue); // clear depth
		gx2WriteGather_submitU32AsBE(stencilClearValue&0xFF); // clear stencil
	}

	// always uses passed depthClearValue/stencilClearValue for clearing, even if clear flags dont specify value updates
	void GX2ClearDepthStencilEx(GX2DepthBuffer* depthBuffer, float depthClearValue, uint8 stencilClearValue, GX2ClearFlags clearFlags)
	{
		GX2ReserveCmdSpace(50);

		if (!depthBuffer && (depthBuffer->surface.width == 0 || depthBuffer->surface.height == 0))
		{
			// Super Smash Bros tries to clear an uninitialized depth surface?
			debug_printf("GX2ClearDepthStencilEx(): Attempting to clear invalid depthbuffer\n");
			return;
		}

		_updateDepthStencilClearRegs(depthClearValue, stencilClearValue, clearFlags);

		uint32 hleClearFlags = 0;
		if ((clearFlags & GX2ClearFlags::CLEAR_DEPTH) != 0)
			hleClearFlags |= 2;
		if ((clearFlags & GX2ClearFlags::CLEAR_STENCIL) != 0)
			hleClearFlags |= 4;

		// send command to clear color, depth and stencil
		if (hleClearFlags != 0)
		{
			gx2WriteGather_submitU32AsBE(pm4HeaderType3(IT_HLE_CLEAR_COLOR_DEPTH_STENCIL, 23));
			gx2WriteGather_submitU32AsBE(hleClearFlags); // color (1), depth (2), stencil (4)
			gx2WriteGather_submitU32AsBE(MPTR_NULL);
			gx2WriteGather_submitU32AsBE(0); // format for color buffer
			gx2WriteGather_submitU32AsBE(0); // tilemode for color buffer
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(memory_virtualToPhysical(depthBuffer->surface.imagePtr));
			gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.format.value());
			gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.tileMode.value());
			gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.width);
			gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.height);
			gx2WriteGather_submitU32AsBE((uint32)depthBuffer->surface.pitch);
			gx2WriteGather_submitU32AsBE(_swapEndianU32(depthBuffer->viewFirstSlice));
			gx2WriteGather_submitU32AsBE(_swapEndianU32(depthBuffer->viewNumSlices));
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);
			gx2WriteGather_submitU32AsBE(0);

			gx2WriteGather_submitU32AsBE(*(uint32*)&depthClearValue); // clear depth
			gx2WriteGather_submitU32AsBE(stencilClearValue & 0xFF); // clear stencil
		}
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