#include "config/GamePadBridgeConfig.h"
#include "config/CemuConfig.h"

XMLGamePadBridgeConfig_t g_gamePadBridgeConfig(&GetConfigHandle);

void GamePadBridgeConfig::Load(XMLConfigParser& parser)
{
	auto section = parser.get("GamePadBridge");
	enabled = section.get("Enabled", false);
}

void GamePadBridgeConfig::Save(XMLConfigParser& parser)
{
	auto section = parser.set("GamePadBridge");
	section.set("Enabled", enabled);
}
