#include "Cafe/OS/common/OSCommon.h"
#include "nlibnss.h"

namespace nlibnss
{
	int NSSExportDeviceCertChain(uint8* uknPtr1, uint32be* uknLength1, uint8* uknPtr2, uint32be* uknLength2, uint32 uknR7, uint32 uknR8)
	{
		cemuLog_logDebug(LogType::Force, "NSSExportDeviceCertChain called but not implemented");
		cemu_assert_debug(false);

		// uknR3 = pointer (can be null, in which case only length is written)
		// uknR4 = length
		// uknR5 = pointer2
		// uknR6 = length2
		// uknR7 = some integer
		// uknR8 = ???

		*uknLength1 = 0x100;
		*uknLength2 = 0x100;


		return 0; // failed
	}

	int NSSSignatureGetSignatureLength()
	{
		// parameters are unknown
		cemu_assert_debug(false);
		return 0x1C; // signature length
	}

	void load()
	{
		cafeExportRegister("nlibnss", NSSSignatureGetSignatureLength, LogType::Placeholder);
		cafeExportRegister("nlibnss", NSSExportDeviceCertChain, LogType::Placeholder);
	}
}
