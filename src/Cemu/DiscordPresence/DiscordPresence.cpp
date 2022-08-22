#include "DiscordPresence.h"

#ifdef ENABLE_DISCORD_RPC

#include <discord_rpc.h>
#include "Common/version.h"

DiscordPresence::DiscordPresence()
{
	DiscordEventHandlers handlers{};
	Discord_Initialize("460807638964371468", &handlers, 1, nullptr);
	UpdatePresence(Idling);
}

DiscordPresence::~DiscordPresence()
{
	ClearPresence();
	Discord_Shutdown();
}

void DiscordPresence::UpdatePresence(State state, const std::string& text) const
{
	DiscordRichPresence discord_presence{};

	std::string state_string, details_string;
	switch (state)
	{
	case Idling:
		details_string = "Idling";
		break;
	case Playing:
		details_string = "Ingame";
		state_string = "Playing " + text;
		break;
	default:
		assert(false);
		break;
	}

	char image_text[128];
	sprintf(image_text, EMULATOR_NAME" %d.%d%s", EMULATOR_VERSION_LEAD, EMULATOR_VERSION_MAJOR, EMULATOR_VERSION_SUFFIX);

	discord_presence.details = details_string.c_str();
	discord_presence.state = state_string.c_str();
	discord_presence.startTimestamp = time(nullptr);
	discord_presence.largeImageText = image_text;
	discord_presence.largeImageKey = "logo_icon_big_png";
	Discord_UpdatePresence(&discord_presence);
}

void DiscordPresence::ClearPresence() const
{
	Discord_ClearPresence();
}

#endif