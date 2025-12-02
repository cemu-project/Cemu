#include "Cafe/OS/RPL/COSModule.h"

namespace TCL
{
	enum class TCLTimestampId
	{
		TIMESTAMP_LAST_BUFFER_RETIRED = 1,
	};

	enum class TCLSubmissionFlag : uint32
	{
		SURFACE_SYNC = 0x400000, // submit surface sync packet before cmd
		NO_MARKER_INTERRUPT = 0x200000,
		USE_RETIRED_MARKER = 0x20000000, // Controls whether the timer is updated before or after (retired) the cmd. Also controls which timestamp is returned for the submission. Before and after using separate counters
	};

	int TCLTimestamp(TCLTimestampId id, uint64be* timestampOut);
	int TCLWaitTimestamp(TCLTimestampId id, uint64 waitTs, uint64 timeout);
	int TCLSubmitToRing(uint32be* cmd, uint32 cmdLen, betype<TCLSubmissionFlag>* controlFlags, uint64be* timestampValueOut);

	// called from Latte code
	bool TCLGPUReadRBWord(uint32& cmdWord);
	void TCLGPUNotifyNewRetirementTimestamp();

	COSModule* GetModule();
}
ENABLE_BITMASK_OPERATORS(TCL::TCLSubmissionFlag);
