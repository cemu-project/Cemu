#pragma once

#include "input/api/ControllerProvider.h"

#ifndef HAS_GAMEPAD_BRIDGE
#define HAS_GAMEPAD_BRIDGE 1
#endif

// The real Wii U GamePad, through the GamePad bridge (fork-only). Input arrives over the bridge's IPC
// (GamePadSink::LatestInput); one controller, always listed so it can be configured before the pad is on.
class GamePadBridgeControllerProvider : public ControllerProviderBase
{
	friend class GamePadBridgeController;
public:
	inline static InputAPI::Type kAPIType = InputAPI::GamePadBridge;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;
};
