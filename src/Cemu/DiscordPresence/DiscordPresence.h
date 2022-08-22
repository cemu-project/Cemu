#pragma once

#ifdef ENABLE_DISCORD_RPC

class DiscordPresence
{
public:
	enum State
	{
		Idling,
		Playing,
	};

	DiscordPresence();
	~DiscordPresence();

	void UpdatePresence(State state, const std::string& text = {}) const;
	void ClearPresence() const;
};

#endif
