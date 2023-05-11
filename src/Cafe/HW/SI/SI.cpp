#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/HW/Common/HwReg.h"
#include "si.h"

namespace HW_SI
{

	struct  
	{
		struct  
		{
			HWREG::SICOMCSR sicomcsr{};
			HWREG::SIPOLL sipoll{};
			HWREG::SICOUTBUF outBuf[4]{};
		}registerState;
		struct
		{
			uint8 cmd{};
			uint8 buf[2]{};
		}outputBufferState[4];
		struct  
		{
			bool hasErrorNoResponse{};
		}channelStatus[4];
	}g_si;

	// normally we should call this periodically according to the parameters set in SIPOLL
	// but for now we just call it whenever status registers are read
	void handlePollUpdate()
	{
		for (uint32 i = 0; i < 4; i++)
		{
			// note: Order of EN and VBCPY is from MSB to LSB
			bool isEnabled = ((g_si.registerState.sipoll.get_EN() >> (3 - i))&1) != 0;
			if (isEnabled)
			{
				g_si.channelStatus[i].hasErrorNoResponse = true;
			}
		}
	}

	void handleQueuedTransfers()
	{
		
	}

	void flushAllOutputBuffers()
	{
		for (uint32 i = 0; i < 4; i++)
		{
			g_si.outputBufferState[i].cmd = g_si.registerState.outBuf[i].get_CMD();
			g_si.outputBufferState[i].buf[0] = g_si.registerState.outBuf[i].get_OUTPUT0();
			g_si.outputBufferState[i].buf[1] = g_si.registerState.outBuf[i].get_OUTPUT1();
		}
	}

	/* +0x6400/0x640C/0x6418/0x6424 | SI0COUTBUF - SI3COUTBUF */

	HWREG::SICOUTBUF SI_COUTBUF_R32(PAddr addr)
	{
		uint32 joyChannelIndex = (addr & 0xFF) / 0xC;

		cemu_assert_debug(false);
		return HWREG::SICOUTBUF();
	}

	void SI_COUTBUF_W32(PAddr addr, HWREG::SICOUTBUF newValue)
	{
		uint32 joyChannelIndex = (addr & 0xFF) / 0xC;
		g_si.registerState.outBuf[joyChannelIndex] = newValue;
	}

	/* +0x6430 | SIPOLL */

	HWREG::SIPOLL SI_POLL_R32(PAddr addr)
	{
		cemu_assert_debug(false);
		return g_si.registerState.sipoll;
	}

	void SI_POLL_W32(PAddr addr, HWREG::SIPOLL newValue)
	{
		g_si.registerState.sipoll = newValue;
	}

	/* +0x6434 | SICOMCSR */

	HWREG::SICOMCSR SI_COMCSR_R32(PAddr addr)
	{
		//cemuLog_logDebug(LogType::Force, "Read SICOMCSR");
		return g_si.registerState.sicomcsr;
	}

	void SI_COMCSR_W32(PAddr addr, HWREG::SICOMCSR newValue)
	{
		uint32 unhandledBits = g_si.registerState.sicomcsr.getRawValue() & ~(0x80000000);
		cemu_assert_debug(unhandledBits == 0);
		// clear transfer complete interrupt
		if (newValue.get_TCINT())
		{
			g_si.registerState.sicomcsr.set_TCINT(0);
		}

		if (newValue.get_TRANSFER_START())
		{
			cemu_assert_debug(false);
			handleQueuedTransfers();
		}
	}

	/* +0x6438 | SISR */

	HWREG::SISR SI_SR_R32(PAddr addr)
	{
		handlePollUpdate();
		HWREG::SISR reg;
		// no response error
		if (g_si.channelStatus[0].hasErrorNoResponse)
			reg.set_NOREP0(1);
		if (g_si.channelStatus[1].hasErrorNoResponse)
			reg.set_NOREP1(1);
		if (g_si.channelStatus[2].hasErrorNoResponse)
			reg.set_NOREP2(1);
		if (g_si.channelStatus[3].hasErrorNoResponse)
			reg.set_NOREP3(1);

		// todo - other status fields

		return reg;
	}

	void SI_SR_W32(PAddr addr, HWREG::SISR newValue)
	{
		if (newValue.get_NOREP0())
			g_si.channelStatus[0].hasErrorNoResponse = false;
		if (newValue.get_NOREP1())
			g_si.channelStatus[1].hasErrorNoResponse = false;
		if (newValue.get_NOREP2())
			g_si.channelStatus[2].hasErrorNoResponse = false;
		if (newValue.get_NOREP3())
			g_si.channelStatus[3].hasErrorNoResponse = false;


		if (newValue.get_WR())
		{
			// copies contents of SICOUTBUF to the internal shadow buffers
			flushAllOutputBuffers();
		}
	}

	void Initialize()
	{
		MMU::RegisterMMIO_32<HWREG::SICOUTBUF, SI_COUTBUF_R32, SI_COUTBUF_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x6400);
		MMU::RegisterMMIO_32<HWREG::SICOUTBUF, SI_COUTBUF_R32, SI_COUTBUF_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x640C);
		MMU::RegisterMMIO_32<HWREG::SICOUTBUF, SI_COUTBUF_R32, SI_COUTBUF_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x6418);
		MMU::RegisterMMIO_32<HWREG::SICOUTBUF, SI_COUTBUF_R32, SI_COUTBUF_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x6424);

		MMU::RegisterMMIO_32<HWREG::SIPOLL, SI_POLL_R32, SI_POLL_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x6430);
		MMU::RegisterMMIO_32<HWREG::SICOMCSR, SI_COMCSR_R32, SI_COMCSR_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x6434);
		MMU::RegisterMMIO_32<HWREG::SISR, SI_SR_R32, SI_SR_W32>(MMU::MMIOInterface::INTERFACE_0D000000, 0x6438);
	}
}
