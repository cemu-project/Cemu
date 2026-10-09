#include "uinput_pad.h"
#include "log.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace drcb {

namespace {
// GamePad button -> Linux gamepad code. Nintendo layout: A is the right face button (BTN_EAST in the
// position-based kernel naming), B bottom, X top, Y left.
struct Map
{
	uint32_t drcb;
	int code;
};
constexpr Map kMap[] = {
	{DRCB_BTN_A, BTN_EAST},       {DRCB_BTN_B, BTN_SOUTH},         {DRCB_BTN_X, BTN_NORTH},       {DRCB_BTN_Y, BTN_WEST},
	{DRCB_BTN_L, BTN_TL},         {DRCB_BTN_R, BTN_TR},            {DRCB_BTN_ZL, BTN_TL2},        {DRCB_BTN_ZR, BTN_TR2},
	{DRCB_BTN_MINUS, BTN_SELECT}, {DRCB_BTN_PLUS, BTN_START},       {DRCB_BTN_HOME, BTN_MODE},
	{DRCB_BTN_STICK_L, BTN_THUMBL}, {DRCB_BTN_STICK_R, BTN_THUMBR},
	{DRCB_BTN_UP, BTN_DPAD_UP},   {DRCB_BTN_DOWN, BTN_DPAD_DOWN},  {DRCB_BTN_LEFT, BTN_DPAD_LEFT}, {DRCB_BTN_RIGHT, BTN_DPAD_RIGHT},
	{DRCB_BTN_TV, BTN_TRIGGER_HAPPY1},
};
constexpr int kAxisMax = 32767;

void emit(int fd, int type, int code, int value)
{
	input_event ev{};
	ev.type = uint16_t(type);
	ev.code = uint16_t(code);
	ev.value = value;
	if (write(fd, &ev, sizeof(ev)) != sizeof(ev))
		LOGD("uinput write failed: %s", strerror(errno));
}
} // namespace

UinputPad::~UinputPad()
{
	if (m_fd >= 0)
	{
		ioctl(m_fd, UI_DEV_DESTROY);
		close(m_fd);
	}
}

bool UinputPad::open()
{
	m_fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (m_fd < 0)
	{
		LOGE("uinput: can't open /dev/uinput (%s). Give your user access (e.g. a udev rule with TAG+=\"uaccess\"), "
			 "or run without --uinput.", strerror(errno));
		return false;
	}
	ioctl(m_fd, UI_SET_EVBIT, EV_KEY);
	for (const auto& m : kMap)
		ioctl(m_fd, UI_SET_KEYBIT, m.code);
	ioctl(m_fd, UI_SET_EVBIT, EV_ABS);
	for (int axis : {ABS_X, ABS_Y, ABS_RX, ABS_RY})
	{
		uinput_abs_setup abs{};
		abs.code = uint16_t(axis);
		abs.absinfo.minimum = -kAxisMax;
		abs.absinfo.maximum = kAxisMax;
		abs.absinfo.flat = 0; // the parser already applies libdrc's dead zone
		ioctl(m_fd, UI_ABS_SETUP, &abs);
	}
	uinput_setup setup{};
	setup.id.bustype = BUS_VIRTUAL;
	setup.id.vendor = 0x057e; // Nintendo's USB vendor id, so SDL picks a Nintendo-style layout where it can
	setup.id.product = 0x0000;
	setup.id.version = 1;
	snprintf(setup.name, sizeof(setup.name), "Wii U GamePad (cemu-gamepad bridge)");
	if (ioctl(m_fd, UI_DEV_SETUP, &setup) < 0 || ioctl(m_fd, UI_DEV_CREATE) < 0)
	{
		LOGE("uinput: device creation failed: %s", strerror(errno));
		close(m_fd);
		m_fd = -1;
		return false;
	}
	LOGI("uinput: created \"%s\"", setup.name);
	return true;
}

void UinputPad::update(const drcb_input_state& s)
{
	if (m_fd < 0)
		return;
	const uint32_t changed = s.buttons ^ m_last_buttons;
	for (const auto& m : kMap)
		if (changed & m.drcb)
			emit(m_fd, EV_KEY, m.code, (s.buttons & m.drcb) ? 1 : 0);
	m_last_buttons = s.buttons;
	// Linux/SDL convention: +y is down; ours is +y up.
	emit(m_fd, EV_ABS, ABS_X, int(std::lround(s.stick_l[0] * kAxisMax)));
	emit(m_fd, EV_ABS, ABS_Y, int(std::lround(-s.stick_l[1] * kAxisMax)));
	emit(m_fd, EV_ABS, ABS_RX, int(std::lround(s.stick_r[0] * kAxisMax)));
	emit(m_fd, EV_ABS, ABS_RY, int(std::lround(-s.stick_r[1] * kAxisMax)));
	emit(m_fd, EV_SYN, SYN_REPORT, 0);
}

} // namespace drcb
