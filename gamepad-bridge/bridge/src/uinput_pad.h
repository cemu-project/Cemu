#pragma once
// Virtual Linux gamepad (/dev/uinput) mirroring the GamePad's buttons and sticks, for apps other than Cemu.
// Cemu gets the pad through the DSU server instead (motion + touch); mapping both in Cemu would double inputs,
// so this is off unless --uinput is given.
#include "drcbridge/ipc.h"

namespace drcb {

class UinputPad
{
public:
	~UinputPad();
	bool open(); // false (logged, with the fix) if /dev/uinput isn't writable
	void update(const drcb_input_state& s);

private:
	int m_fd = -1;
	uint32_t m_last_buttons = 0;
};

} // namespace drcb
