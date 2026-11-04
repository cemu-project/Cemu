#include "Cafe/OS/RPL/COSModule.h"

namespace nsyskbd
{
	COSModule* GetModule();

	void SetHostKeyState(uint8 usage, bool pressed);
	void ReleaseHostKeys();
	bool IsHostKeyboardConnected();
}

