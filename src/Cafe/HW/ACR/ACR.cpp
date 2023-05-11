#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/HW/Common/HwReg.h"

namespace HW_ACR
{

	struct  
	{
		HWREG::ACR_VI_ADDR viAddr;
		HWREG::ACR_VI_CTRL viCtrl;
	}g_acr;

	/* 
	Is this some kind of VI emulation interface? 
	Pattern seen in Twilight Princess HD:
	- If Hollywood hardware then read/write old 16bit GC VI register directly
	- Otherwise these steps are performed:
		VICONTROL |= 1
		VIADDR = registerIndex
		VIDATA = data
		VICONTROL &= ~1
		All the register accesses here are 32bit
	*/

	/* 0x0D00021C | Accesses VI register currently selected by VIADDR */
	HWREG::ACR_VI_DATA ACR_VIDATA_R32(PAddr addr)
	{
		cemuLog_logDebug(LogType::Force, "ACR_VIDATA read with selected reg {:08x}", g_acr.viAddr.get_ADDR());
		return HWREG::ACR_VI_DATA();
	}

	void ACR_VIDATA_W32(PAddr addr, HWREG::ACR_VI_DATA newValue)
	{
		cemuLog_logDebug(LogType::Force, "ACR_VIDATA write {:08x} with selected reg {:08x}", newValue.get_DATA(), g_acr.viAddr.get_ADDR());
	}

	/* 0x0D000224 | Controls the selected VI register? */
	HWREG::ACR_VI_ADDR ACR_VIADDR_R32(PAddr addr)
	{
		return g_acr.viAddr;
	}

	void ACR_VIADDR_W32(PAddr addr, HWREG::ACR_VI_ADDR newValue)
	{
		g_acr.viAddr = newValue;
	}

	/* 0x0D000228 | Some kind of VI interface control? */
	HWREG::ACR_VI_CTRL ACR_VICONTROL_R32(PAddr addr)
	{
		return g_acr.viCtrl;
	}

	void ACR_VICONTROL_W32(PAddr addr, HWREG::ACR_VI_CTRL newValue)
	{
		g_acr.viCtrl = newValue;
	}

	RunAtCemuBoot _initACR([]()
	{
		MMU::RegisterMMIO_32<HWREG::ACR_VI_DATA, ACR_VIDATA_R32, ACR_VIDATA_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x21C);
		MMU::RegisterMMIO_32<HWREG::ACR_VI_ADDR, ACR_VIADDR_R32, ACR_VIADDR_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x224);
		MMU::RegisterMMIO_32<HWREG::ACR_VI_CTRL, ACR_VICONTROL_R32, ACR_VICONTROL_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x228);

		// init 
	});

}
