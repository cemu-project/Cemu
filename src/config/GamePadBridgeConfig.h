#pragma once

#include "config/XMLConfig.h"

// Settings for the real Wii U GamePad bridge (fork-only).
// Stored as a <GamePadBridge> section inside Cemu's settings.xml via XMLChildConfig,
// the same mechanism wxCemuConfig uses, so CemuConfig.cpp needs no changes.
struct GamePadBridgeConfig
{
	// Master switch. When enabled and the bridge is unreachable at game start,
	// Cemu shows an on-screen notice and runs the game without the pad.
	bool enabled = false;

	// Present gate (Task 7): hold the TV picture this long so it lines up with the GamePad screen.
	// Applied in whole frames (round(ms / 16.67)), only while the bridge is connected. Set by hand for
	// now (a "fake delay"); later from the pad's measured display latency.
	sint32 tvHoldMs = 0;
	static constexpr sint32 kMaxTvHoldMs = 100;

	// Replace both screens with the camera sync pattern: a white flash once a second plus a frame
	// counter (docs/SYNC.md). Goes through the gate and the pad path like normal frames.
	bool syncTestPattern = false;

	// Games that draw nothing on the GamePad (Smash Bros.): after a second without a GamePad frame, show the
	// TV picture on the pad instead of a status screen. Not what a real Wii U does; the user's choice.
	bool mirrorTvWhenPadUnused = true;

	void Load(XMLConfigParser& parser);
	void Save(XMLConfigParser& parser);
};

typedef XMLChildConfig<GamePadBridgeConfig, &GamePadBridgeConfig::Load, &GamePadBridgeConfig::Save> XMLGamePadBridgeConfig_t;

extern XMLGamePadBridgeConfig_t g_gamePadBridgeConfig;

inline XMLGamePadBridgeConfig_t& GetGamePadBridgeConfigHandle()
{
	return g_gamePadBridgeConfig;
}

inline GamePadBridgeConfig& GetGamePadBridgeConfig()
{
	return GetGamePadBridgeConfigHandle().Data();
}
