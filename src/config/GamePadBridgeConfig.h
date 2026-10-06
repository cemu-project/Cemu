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
