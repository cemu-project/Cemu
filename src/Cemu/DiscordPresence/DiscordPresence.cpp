#include "DiscordPresence.h"
#include "Common/version.h"
#include "DiscordRPCLite.h"

DiscordPresence::DiscordPresence()
{
	m_rpcClient = new DiscordRPCLite("460807638964371468");
	UpdatePresence(Idling);
}

DiscordPresence::~DiscordPresence()
{
	ClearPresence();
	if (m_rpcClient)
		delete m_rpcClient;
}

void DiscordPresence::UpdatePresence(State state, const std::string& text) const
{
	if (!m_rpcClient)
		return;
	DiscordLiteRichPresence discordPresence{};
	std::string stateString, detailsString;
	switch (state)
	{
	case Idling:
		detailsString = "Idling";
		stateString = "";
		break;
	case Playing:
		detailsString = "Ingame";
		stateString = "Playing " + text;
		break;
	default:
		assert(false);
		break;
	}
	discordPresence.details = detailsString;
	discordPresence.state = stateString;
	discordPresence.startTimestamp = time(nullptr);
	discordPresence.largeImageText = BUILD_VERSION_WITH_NAME_STRING;
	discordPresence.largeImageKey = "logo_icon_big_png";
	m_rpcClient->UpdateRichPresence(discordPresence);
}

void DiscordPresence::ClearPresence() const
{
	if (!m_rpcClient)
		return;
	m_rpcClient->ClearRichPresence();
}