#pragma once
#include "Cafe/OS/RPL/COSModule.h"

namespace nsyshid
{
	class Backend;

	void AttachBackend(const std::shared_ptr<Backend>& backend);
	void DetachBackend(const std::shared_ptr<Backend>& backend);

	void UpdateEmulatedDevices(); // after their settings changed

	void AddHostMouseMotion(sint32 deltaX, sint32 deltaY, sint32 wheel);
	void SetHostMouseButton(uint8 button, bool pressed);
	void ReleaseHostMouse();
	bool IsMouseInUse(); // a title reads from the emulated mouse

	COSModule* GetModule();
} // namespace nsyshid
