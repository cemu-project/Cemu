#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/TCL/TCL.h"

namespace TCL
{

	enum class TCL_SUBMISSION_FLAG : uint32
	{
		SURFACE_SYNC = 0x400000, // submit surface sync packet before cmd
		TRIGGER_INTERRUPT = 0x200000, // probably
		UKN_20000000 = 0x20000000,
	};

	int TCLSubmitToRing(uint32be* cmd, uint32 cmdLen, uint32be* controlFlags, uint64* submissionTimestamp)
	{
		// todo - figure out all the bits of *controlFlags
		// if submissionTimestamp != nullptr then set it to the timestamp of the submission. Note: We should make sure that uint64's are written atomically by the GPU command processor

		cemu_assert_debug(false);

		return 0;
	}

	void Initialize()
	{
		cafeExportRegister("TCL", TCLSubmitToRing, LogType::Placeholder);
	}
}
