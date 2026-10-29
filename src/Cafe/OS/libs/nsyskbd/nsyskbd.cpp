#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Common/CafeStaticVar.h"
#include "Cafe/CafeSystem.h"
#include "config/CemuConfig.h"
#include "nsyskbd.h"

// the host keyboard is channel 0, keys are always translated with the US layout
namespace nsyskbd
{
	constinit COSStaticData s_kbdStaticData;

	enum KBDResult : sint32
	{
		KBD_RESULT_SUCCESS = 0,
		KBD_RESULT_NOT_INITIALIZED = 2,
		KBD_RESULT_ALREADY_INITIALIZED = 3,
		KBD_RESULT_INVALID_PARAM = 4,
		KBD_RESULT_NOT_CONNECTED = 5,
	};

	enum KBDChannelStatus : uint32
	{
		KBD_CHANNEL_STATUS_CONNECTED = 0,
		KBD_CHANNEL_STATUS_DISCONNECTED = 1,
	};

	enum KBDKeyState : uint32
	{
		KBD_KEY_STATE_UP = 0,
		KBD_KEY_STATE_DOWN = 1,
		KBD_KEY_STATE_REPEAT = 3,
	};

	enum KBDModState : uint32
	{
		KBD_MS_CTRL = 0x1,
		KBD_MS_SHIFT = 0x2,
		KBD_MS_ALT = 0x4,
		KBD_MS_GUI = 0x8,
		KBD_MS_EXTRA = 0x10,
		KBD_MS_ALTGR = 0x20,
		KBD_MS_NUM_LOCK = 0x100,
		KBD_MS_CAPS_LOCK = 0x200,
		KBD_MS_SCROLL_LOCK = 0x400,
		KBD_MS_KEEP_CTRL_CHARS = 0x800, // ctrl/alt/gui don't drop the shift state during translation
		KBD_MS_SET_ALL = 0x1000, // KBDSetModState overwrites all bits instead of just the lock bits

		KBD_MS_LOCK_MASK = 0xFC0,
		KBD_MS_PRESSED_MASK = 0x3F,
	};

	// special unicode values for keys without a character
	enum KBDSpecialKey : uint16
	{
		KBK_CTRL = 0xF000,
		KBK_SHIFT = 0xF001,
		KBK_ALT = 0xF002,
		KBK_GUI = 0xF003,
		KBK_NUM_LOCK = 0xF007,
		KBK_CAPS_LOCK = 0xF008,
		KBK_PRINT_SCREEN = 0xF020,
		KBK_SCROLL_LOCK = 0xF021,
		KBK_PAUSE = 0xF022,
		KBK_APPLICATION = 0xF02F,
		KBK_INVALID = 0xFFFF,
	};

	constexpr uint32 KBD_MAX_CHANNELS = 4;
	constexpr uint32 KBD_QUEUE_SIZE = 32;
	constexpr uint8 KBD_HID_QUEUE_OVERFLOW = 0xFF;
	constexpr sint32 KBD_COUNTRY_US = 17;
	constexpr sint32 KBD_COUNTRY_COUNT = 19;
	constexpr uint32 KBD_POLL_INTERVAL_MS = 8;

	struct KBDKeyEvent
	{
		/* +0x00 */ uint8be channel;
		/* +0x01 */ uint8be hidCode; // 0 if the queue is empty, 0xFF if events were dropped
		/* +0x02 */ uint8be padding02[2];
		/* +0x04 */ uint32be state;
		/* +0x08 */ uint32be modState;
		/* +0x0C */ uint16be unicode;
		/* +0x0E */ uint8be padding0E[2];
	};
	static_assert(sizeof(KBDKeyEvent) == 0x10);

	struct KBDChannelInfo
	{
		uint8be channel;
	};

	/* US layout */

	enum KeyType : uint16
	{
		KEY_NORMAL,
		KEY_LETTER, // caps lock inverts shift
		KEY_KEYPAD, // num lock selects the digit
		KEY_FIXED, // same code regardless of the modifiers
	};

	// {type, unshifted, shifted} for usage 0x04 to 0x67
	constexpr uint16 s_usKeys[100][3] = {
		{KEY_LETTER, 'a', 'A'}, {KEY_LETTER, 'b', 'B'}, {KEY_LETTER, 'c', 'C'}, {KEY_LETTER, 'd', 'D'}, // 0x04
		{KEY_LETTER, 'e', 'E'}, {KEY_LETTER, 'f', 'F'}, {KEY_LETTER, 'g', 'G'}, {KEY_LETTER, 'h', 'H'}, // 0x08
		{KEY_LETTER, 'i', 'I'}, {KEY_LETTER, 'j', 'J'}, {KEY_LETTER, 'k', 'K'}, {KEY_LETTER, 'l', 'L'}, // 0x0C
		{KEY_LETTER, 'm', 'M'}, {KEY_LETTER, 'n', 'N'}, {KEY_LETTER, 'o', 'O'}, {KEY_LETTER, 'p', 'P'}, // 0x10
		{KEY_LETTER, 'q', 'Q'}, {KEY_LETTER, 'r', 'R'}, {KEY_LETTER, 's', 'S'}, {KEY_LETTER, 't', 'T'}, // 0x14
		{KEY_LETTER, 'u', 'U'}, {KEY_LETTER, 'v', 'V'}, {KEY_LETTER, 'w', 'W'}, {KEY_LETTER, 'x', 'X'}, // 0x18
		{KEY_LETTER, 'y', 'Y'}, {KEY_LETTER, 'z', 'Z'}, {KEY_NORMAL, '1', '!'}, {KEY_NORMAL, '2', '@'}, // 0x1C
		{KEY_NORMAL, '3', '#'}, {KEY_NORMAL, '4', '$'}, {KEY_NORMAL, '5', '%'}, {KEY_NORMAL, '6', '^'}, // 0x20
		{KEY_NORMAL, '7', '&'}, {KEY_NORMAL, '8', '*'}, {KEY_NORMAL, '9', '('}, {KEY_NORMAL, '0', ')'}, // 0x24
		{KEY_NORMAL, 0xF1CD, 0xF1CD}, {KEY_NORMAL, 0xF1DB, 0xF1DB}, {KEY_NORMAL, 0xF1C8, 0xF1C8}, {KEY_NORMAL, 0xF1C9, 0xF1C9}, // 0x28 enter, escape, backspace, tab
		{KEY_NORMAL, ' ', ' '}, {KEY_NORMAL, '-', '_'}, {KEY_NORMAL, '=', '+'}, {KEY_NORMAL, '[', '{'}, // 0x2C
		{KEY_NORMAL, ']', '}'}, {KEY_NORMAL, '\\', '|'}, {KEY_NORMAL, 0, 0}, {KEY_NORMAL, ';', ':'}, // 0x30
		{KEY_NORMAL, '\'', '"'}, {KEY_NORMAL, '`', '~'}, {KEY_NORMAL, ',', '<'}, {KEY_NORMAL, '.', '>'}, // 0x34
		{KEY_NORMAL, '/', '?'}, {KEY_FIXED, KBK_CAPS_LOCK, KBK_CAPS_LOCK}, {KEY_NORMAL, 0xF061, 0xF061}, {KEY_NORMAL, 0xF062, 0xF062}, // 0x38 caps lock, F1, F2
		{KEY_NORMAL, 0xF063, 0xF063}, {KEY_NORMAL, 0xF064, 0xF064}, {KEY_NORMAL, 0xF065, 0xF065}, {KEY_NORMAL, 0xF066, 0xF066}, // 0x3C F3 - F6
		{KEY_NORMAL, 0xF067, 0xF067}, {KEY_NORMAL, 0xF068, 0xF068}, {KEY_NORMAL, 0xF069, 0xF069}, {KEY_NORMAL, 0xF06A, 0xF06A}, // 0x40 F7 - F10
		{KEY_NORMAL, 0xF06B, 0xF06B}, {KEY_NORMAL, 0xF06C, 0xF06C}, {KEY_FIXED, KBK_PRINT_SCREEN, KBK_PRINT_SCREEN}, {KEY_FIXED, KBK_SCROLL_LOCK, KBK_SCROLL_LOCK}, // 0x44 F11, F12
		{KEY_FIXED, KBK_PAUSE, KBK_PAUSE}, {KEY_NORMAL, 0xF1B0, 0xF1B0}, {KEY_NORMAL, 0xF1B7, 0xF1B7}, {KEY_NORMAL, 0xF1B9, 0xF1B9}, // 0x48 pause, insert, home, page up
		{KEY_NORMAL, 0xF1AE, 0xF1AE}, {KEY_NORMAL, 0xF1B1, 0xF1B1}, {KEY_NORMAL, 0xF1B3, 0xF1B3}, {KEY_NORMAL, 0xF1B6, 0xF1B6}, // 0x4C delete, end, page down, right
		{KEY_NORMAL, 0xF1B4, 0xF1B4}, {KEY_NORMAL, 0xF1B2, 0xF1B2}, {KEY_NORMAL, 0xF1B8, 0xF1B8}, {KEY_FIXED, KBK_NUM_LOCK, KBK_NUM_LOCK}, // 0x50 left, down, up, num lock
		{KEY_NORMAL, 0xF12F, 0xF12F}, {KEY_NORMAL, 0xF12A, 0xF12A}, {KEY_NORMAL, 0xF12D, 0xF12D}, {KEY_NORMAL, 0xF12B, 0xF12B}, // 0x54 keypad / * - +
		{KEY_NORMAL, 0xF10D, 0xF10D}, {KEY_KEYPAD, 0xF171, 0xF131}, {KEY_KEYPAD, 0xF172, 0xF132}, {KEY_KEYPAD, 0xF173, 0xF133}, // 0x58 keypad enter, 1 - 3
		{KEY_KEYPAD, 0xF174, 0xF134}, {KEY_KEYPAD, 0xF175, 0xF135}, {KEY_KEYPAD, 0xF176, 0xF136}, {KEY_KEYPAD, 0xF177, 0xF137}, // 0x5C keypad 4 - 7
		{KEY_KEYPAD, 0xF178, 0xF138}, {KEY_KEYPAD, 0xF179, 0xF139}, {KEY_KEYPAD, 0xF170, 0xF130}, {KEY_KEYPAD, 0xF16E, 0xF12E}, // 0x60 keypad 8, 9, 0, .
		{KEY_NORMAL, 0, 0}, {KEY_FIXED, KBK_APPLICATION, KBK_APPLICATION}, {KEY_NORMAL, 0, 0}, {KEY_NORMAL, 0xF13D, 0xF13D}, // 0x64 non-us \, application, power, keypad =
	};

	// left and right ctrl, shift, alt, gui
	constexpr uint16 s_usModifiers[4] = {KBK_CTRL, KBK_SHIFT, KBK_ALT, KBK_GUI};

	uint16 TranslateHidCode(uint8 hidCode, uint32 modState)
	{
		if (hidCode < 4)
			return KBK_INVALID;
		if (hidCode >= 0xE0 && hidCode < 0xE8)
			return s_usModifiers[(hidCode - 0xE0) % 4];
		if (hidCode >= 4 + std::size(s_usKeys))
			return 0;
		const uint16* entry = s_usKeys[hidCode - 4];
		if (entry[0] == KEY_FIXED)
			return entry[1];
		// the US layout has no altgr characters
		if ((modState & KBD_MS_ALTGR) || (modState & (KBD_MS_CTRL | KBD_MS_ALT)) == (KBD_MS_CTRL | KBD_MS_ALT))
			return 0;
		if ((modState & KBD_MS_KEEP_CTRL_CHARS) == 0 && (modState & (KBD_MS_CTRL | KBD_MS_ALT | KBD_MS_GUI | KBD_MS_EXTRA)) != 0)
			return entry[1];
		bool shifted = (modState & KBD_MS_SHIFT) != 0;
		if (entry[0] == KEY_LETTER && (modState & KBD_MS_CAPS_LOCK))
			shifted = !shifted;
		else if (entry[0] == KEY_KEYPAD)
			shifted = (modState & KBD_MS_NUM_LOCK) && !shifted;
		return entry[shifted ? 2 : 1];
	}

	/* channels */

	struct KBDChannel
	{
		uint8 index;
		uint32 status;
		sint32 country;
		uint32 lockProcessing;
		uint32 modState;
		sint32 modCounters[4]; // held ctrl, shift, alt and gui keys
		uint16 repeatDelay;
		uint16 repeatRate;
		uint8 repeatHidCode;
		uint64 repeatTime;
		std::bitset<256> keysDown;
		std::deque<KBDKeyEvent> queue;
		std::vector<KBDKeyEvent> pendingEvents; // host keys not yet seen by the poll alarm
	};

	static std::mutex s_mutex;
	static KBDChannel s_channels[KBD_MAX_CHANNELS];
	static bool s_kbdSetup{false};
	static MPTR s_attachCallback{MPTR_NULL};
	static MPTR s_detachCallback{MPTR_NULL};
	static MPTR s_keyCallback{MPTR_NULL};
	static CafeStaticVar<coreinit::OSAlarm_t, 1> s_pollAlarm{s_kbdStaticData};
	static std::atomic_bool s_hostKeyboardConnected = false;

	void ResetChannel(KBDChannel& channel)
	{
		channel.modState = 0;
		std::fill(std::begin(channel.modCounters), std::end(channel.modCounters), 0);
		channel.repeatHidCode = 0;
		channel.keysDown.reset();
		channel.queue.clear();
		channel.pendingEvents.clear();
	}

	void InitChannel(KBDChannel& channel, uint8 index)
	{
		channel.index = index;
		channel.status = KBD_CHANNEL_STATUS_DISCONNECTED;
		channel.country = KBD_COUNTRY_US;
		channel.lockProcessing = 1;
		channel.repeatDelay = 500;
		channel.repeatRate = 33;
		ResetChannel(channel);
	}

	KBDChannel* GetChannel(uint32 channelIndex, std::string_view funcName)
	{
		// callers report success for invalid channels
		if (channelIndex >= KBD_MAX_CHANNELS)
		{
			cemuLog_log(LogType::APIErrors, "{}(): Invalid channel {}", funcName, channelIndex);
			return nullptr;
		}
		return s_channels + channelIndex;
	}

	void HandleModifierKey(KBDChannel& channel, uint16 code, bool isDown)
	{
		auto updateModifier = [&](uint32 counterIndex, uint32 modBit) {
			channel.modCounters[counterIndex] += isDown ? 1 : -1;
			channel.modState = (channel.modState & ~modBit) | (channel.modCounters[counterIndex] > 0 ? modBit : 0);
		};
		auto toggleLock = [&](uint32 modBit) {
			if (isDown && channel.lockProcessing)
				channel.modState ^= modBit;
		};
		switch (code)
		{
		case KBK_CTRL:
			updateModifier(0, KBD_MS_CTRL);
			break;
		case KBK_SHIFT:
			updateModifier(1, KBD_MS_SHIFT);
			break;
		case KBK_ALT:
			updateModifier(2, KBD_MS_ALT);
			break;
		case KBK_GUI:
			updateModifier(3, KBD_MS_GUI);
			break;
		case KBK_NUM_LOCK:
			toggleLock(KBD_MS_NUM_LOCK);
			break;
		case KBK_CAPS_LOCK:
			toggleLock(KBD_MS_CAPS_LOCK);
			break;
		case KBK_SCROLL_LOCK:
			toggleLock(KBD_MS_SCROLL_LOCK);
			break;
		default:
			break;
		}
	}

	bool IsRepeatableKey(uint16 code)
	{
		switch (code)
		{
		case KBK_CAPS_LOCK:
		case KBK_NUM_LOCK:
		case KBK_SCROLL_LOCK:
		case KBK_PRINT_SCREEN:
		case KBK_PAUSE:
		case KBK_APPLICATION:
			return false;
		default:
			return true;
		}
	}

	void ProcessKey(KBDChannel& channel, uint8 hidCode, uint32 keyState, std::vector<KBDKeyEvent>& events)
	{
		// the US layout reports the non-US # key as backslash
		if (hidCode == 0x32)
			hidCode = 0x31;
		const uint16 code = TranslateHidCode(hidCode, channel.modState);
		KBDKeyEvent& keyEvent = events.emplace_back();
		keyEvent = {};
		keyEvent.channel = channel.index;
		keyEvent.hidCode = hidCode;
		keyEvent.state = keyState;
		keyEvent.modState = channel.modState;
		keyEvent.unicode = code;

		const bool isDown = (keyState & 1) != 0;
		if (keyState != KBD_KEY_STATE_REPEAT)
			HandleModifierKey(channel, code, isDown);
		// modifiers don't interrupt the repeating key
		if (code >= KBK_CTRL && code <= KBK_GUI)
			return;
		if (isDown)
		{
			channel.repeatHidCode = 0;
			const uint16 delay = keyState == KBD_KEY_STATE_REPEAT ? channel.repeatRate : channel.repeatDelay;
			if (IsRepeatableKey(code) && delay != 0)
			{
				channel.repeatHidCode = hidCode;
				channel.repeatTime = coreinit::OSGetTime() + (uint64)delay * ESPRESSO_TIMER_CLOCK / 1000;
			}
		}
		else if (hidCode == channel.repeatHidCode)
		{
			channel.repeatHidCode = 0;
		}
	}

	void Enqueue(KBDChannel& channel, const KBDKeyEvent& keyEvent)
	{
		if (channel.queue.size() >= KBD_QUEUE_SIZE)
			return;
		if (channel.queue.size() == KBD_QUEUE_SIZE - 1)
		{
			// the last free entry signals that events were dropped
			KBDKeyEvent& overflowEvent = channel.queue.emplace_back();
			overflowEvent = {};
			overflowEvent.channel = keyEvent.channel;
			overflowEvent.hidCode = KBD_HID_QUEUE_OVERFLOW;
			return;
		}
		channel.queue.push_back(keyEvent);
	}

	/* host keyboard */

	void SetHostChannelStatus(uint32 status)
	{
		s_channels[0].status = status;
		const bool connected = status == KBD_CHANNEL_STATUS_CONNECTED;
		if (s_hostKeyboardConnected.exchange(connected) != connected)
			CafeSystem::NotifyUSBInputChanged();
	}

	bool IsHostKeyboardConnected()
	{
		return s_hostKeyboardConnected;
	}

	void SetHostKeyState(uint8 usage, bool pressed)
	{
		std::unique_lock _l(s_mutex);
		KBDChannel& channel = s_channels[0];
		if (channel.status != KBD_CHANNEL_STATUS_CONNECTED)
			return;
		// key repeat, or the release of a key which was already held when the keyboard was connected
		if (channel.keysDown.test(usage) == pressed)
			return;
		channel.keysDown.set(usage, pressed);
		ProcessKey(channel, usage, pressed ? KBD_KEY_STATE_DOWN : KBD_KEY_STATE_UP, channel.pendingEvents);
	}

	void ReleaseHostKeys()
	{
		std::unique_lock _l(s_mutex);
		KBDChannel& channel = s_channels[0];
		if (channel.status != KBD_CHANNEL_STATUS_CONNECTED)
			return;
		for (uint32 usage = 0; usage < channel.keysDown.size(); usage++)
		{
			if (channel.keysDown.test(usage))
				ProcessKey(channel, (uint8)usage, KBD_KEY_STATE_UP, channel.pendingEvents);
		}
		channel.keysDown.reset();
	}

	void CallChannelCallback(MPTR callback, uint8 channelIndex)
	{
		if (callback == MPTR_NULL)
			return;
		StackAllocator<KBDChannelInfo> channelInfo;
		channelInfo->channel = channelIndex;
		PPCCoreCallback(callback, channelInfo.GetPointer());
	}

	void ppcPollAlarmHandler(PPCInterpreter_t* hCPU)
	{
		const bool keyboardEnabled = GetConfig().emulated_usb_devices.emulate_keyboard;
		MPTR channelCallback = MPTR_NULL;
		MPTR keyCallback;
		std::vector<KBDKeyEvent> events;
		{
			std::unique_lock _l(s_mutex);
			if (!s_kbdSetup)
			{
				osLib_returnFromFunction(hCPU, 0);
				return;
			}
			KBDChannel& channel = s_channels[0];
			const bool connected = channel.status == KBD_CHANNEL_STATUS_CONNECTED;
			if (keyboardEnabled && !connected)
			{
				ResetChannel(channel);
				SetHostChannelStatus(KBD_CHANNEL_STATUS_CONNECTED);
				channelCallback = s_attachCallback;
			}
			else if (!keyboardEnabled && connected)
			{
				SetHostChannelStatus(KBD_CHANNEL_STATUS_DISCONNECTED);
				channelCallback = s_detachCallback;
			}
			else if (connected)
			{
				if (channel.repeatHidCode != 0 && coreinit::OSGetTime() >= channel.repeatTime)
					ProcessKey(channel, channel.repeatHidCode, KBD_KEY_STATE_REPEAT, channel.pendingEvents);
				events.swap(channel.pendingEvents);
			}
			keyCallback = s_keyCallback;
		}
		CallChannelCallback(channelCallback, 0);
		// the key callback runs before each event is queued, and outside the lock since it can call back into KBD
		for (const KBDKeyEvent& keyEvent : events)
		{
			if (keyCallback != MPTR_NULL)
			{
				StackAllocator<KBDKeyEvent> callbackEvent;
				*callbackEvent.GetPointer() = keyEvent;
				PPCCoreCallback(keyCallback, callbackEvent.GetPointer());
			}
			std::unique_lock _l(s_mutex);
			Enqueue(s_channels[0], keyEvent);
		}
		osLib_returnFromFunction(hCPU, 0);
	}

	/* exports */

	sint32 KBDSetup(MPTR attachCallback, MPTR detachCallback, MPTR keyCallback)
	{
		{
			std::unique_lock _l(s_mutex);
			if (s_kbdSetup)
				return KBD_RESULT_ALREADY_INITIALIZED;
			s_kbdSetup = true;
			s_attachCallback = attachCallback;
			s_detachCallback = detachCallback;
			s_keyCallback = keyCallback;
		}
		const uint64 pollInterval = (uint64)ESPRESSO_TIMER_CLOCK * KBD_POLL_INTERVAL_MS / 1000;
		coreinit::OSCreateAlarm(s_pollAlarm.GetPtr());
		coreinit::OSSetPeriodicAlarm(s_pollAlarm.GetPtr(), coreinit::OSGetTime() + pollInterval, pollInterval, RPLLoader_MakePPCCallable(ppcPollAlarmHandler));
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDInit(uint32 ukn, MPTR attachCallback, MPTR detachCallback, MPTR keyCallback)
	{
		return KBDSetup(attachCallback, detachCallback, keyCallback);
	}

	sint32 KBDTeardown()
	{
		{
			std::unique_lock _l(s_mutex);
			if (!s_kbdSetup)
				return KBD_RESULT_NOT_INITIALIZED;
			s_kbdSetup = false;
			SetHostChannelStatus(KBD_CHANNEL_STATUS_DISCONNECTED);
			s_attachCallback = MPTR_NULL;
			s_detachCallback = MPTR_NULL;
			s_keyCallback = MPTR_NULL;
		}
		coreinit::OSCancelAlarm(s_pollAlarm.GetPtr());
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDExit()
	{
		return KBDTeardown();
	}

	sint32 KBDGetChannelStatus(uint32 channelIndex, uint32be* status)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDGetChannelStatus");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		*status = channel->status;
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDGetKey(uint32 channelIndex, KBDKeyEvent* keyEvent)
	{
		// used by MSX VC
		KBDChannel* channel = GetChannel(channelIndex, "KBDGetKey");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		if (channel->status != KBD_CHANNEL_STATUS_CONNECTED)
			return KBD_RESULT_NOT_CONNECTED;
		if (channel->queue.empty())
		{
			*keyEvent = {};
			keyEvent->channel = channel->index;
			return KBD_RESULT_SUCCESS;
		}
		*keyEvent = channel->queue.front();
		channel->queue.pop_front();
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDEmptyQueue(uint32 channelIndex)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDEmptyQueue");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		channel->queue.clear();
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDResetChannel(uint32 channelIndex)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDResetChannel");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		ResetChannel(*channel);
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDSetCountry(uint32 channelIndex, sint32 country)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDSetCountry");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		if (country < 0 || country >= KBD_COUNTRY_COUNT)
			return KBD_RESULT_INVALID_PARAM;
		if (country != KBD_COUNTRY_US)
			cemuLog_logOnce(LogType::Placeholder, "KBDSetCountry(): Only the US layout is supported, keys are translated with it instead of country {}", country);
		std::unique_lock _l(s_mutex);
		channel->country = country;
		ResetChannel(*channel);
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDGetCountry(uint32 channelIndex, uint32be* country)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDGetCountry");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		*country = channel->country;
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDSetLockProcessing(uint32 channelIndex, uint32 enable)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDSetLockProcessing");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		channel->lockProcessing = enable;
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDSetModState(uint32 channelIndex, uint32 modState)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDSetModState");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		if (modState & KBD_MS_SET_ALL)
			channel->modState = modState & ~KBD_MS_SET_ALL;
		else
			channel->modState = (modState & KBD_MS_LOCK_MASK) | (channel->modState & KBD_MS_PRESSED_MASK);
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDGetModState(uint32 channelIndex, uint32be* modState)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDGetModState");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		*modState = channel->modState;
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDSetRepeat(uint32 channelIndex, uint16 delay, uint16 rate)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDSetRepeat");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		channel->repeatDelay = delay;
		channel->repeatRate = rate;
		return KBD_RESULT_SUCCESS;
	}

	sint32 KBDGetRepeat(uint32 channelIndex, uint16be* delay, uint16be* rate)
	{
		KBDChannel* channel = GetChannel(channelIndex, "KBDGetRepeat");
		if (!channel)
			return KBD_RESULT_SUCCESS;
		std::unique_lock _l(s_mutex);
		*delay = channel->repeatDelay;
		*rate = channel->repeatRate;
		return KBD_RESULT_SUCCESS;
	}

	uint16 KBDTranslateHidCode(uint8 hidCode, uint32 modState, sint32 country)
	{
		if (country < 0 || country >= KBD_COUNTRY_COUNT)
			return KBK_INVALID;
		return TranslateHidCode(hidCode, modState);
	}

	void Reset()
	{
		std::unique_lock _l(s_mutex);
		for (uint8 i = 0; i < KBD_MAX_CHANNELS; i++)
			InitChannel(s_channels[i], i);
		if (s_hostKeyboardConnected.exchange(false))
			CafeSystem::NotifyUSBInputChanged();
		s_kbdSetup = false;
		s_attachCallback = MPTR_NULL;
		s_detachCallback = MPTR_NULL;
		s_keyCallback = MPTR_NULL;
	}

	class : public COSModule
	{
		public:
		std::string_view GetName() override
		{
			return "nsyskbd";
		}

		void RPLMapped() override
		{
			s_kbdStaticData.Allocate();
			cafeExportRegister("nsyskbd", KBDSetup, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDInit, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDTeardown, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDExit, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDGetChannelStatus, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDGetKey, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDEmptyQueue, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDResetChannel, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDSetCountry, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDGetCountry, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDSetLockProcessing, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDSetModState, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDGetModState, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDSetRepeat, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDGetRepeat, LogType::Placeholder);
			cafeExportRegister("nsyskbd", KBDTranslateHidCode, LogType::Placeholder);
		};

		void RPLUnmapped() override
		{
			coreinit::OSCancelAlarm(s_pollAlarm.GetPtr());
			s_kbdStaticData.Deallocate();
		}

		void rpl_entry(uint32 moduleHandle, coreinit::RplEntryReason reason) override
		{
			// a title can exit without tearing the keyboard down
			if (reason == coreinit::RplEntryReason::Loaded || reason == coreinit::RplEntryReason::Unloaded)
				Reset();
		}
	}s_COSnsyskbdModule;

	COSModule* GetModule()
	{
		return &s_COSnsyskbdModule;
	}
}
