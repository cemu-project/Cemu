#include "config/GamePadBridgeConfig.h"
#include "config/CemuConfig.h"

XMLGamePadBridgeConfig_t g_gamePadBridgeConfig(&GetConfigHandle);

void GamePadBridgeConfig::Load(XMLConfigParser& parser)
{
	auto section = parser.get("GamePadBridge");
	enabled = section.get("Enabled", false);
	tvHoldMs = std::clamp<sint32>(section.get("TvHoldMs", 0), 0, kMaxTvHoldMs);
	syncTestPattern = section.get("SyncTestPattern", false);
}

void GamePadBridgeConfig::Save(XMLConfigParser& parser)
{
	auto section = parser.set("GamePadBridge");
	section.set("Enabled", enabled);
	section.set("TvHoldMs", tvHoldMs);
	section.set("SyncTestPattern", syncTestPattern);
}
