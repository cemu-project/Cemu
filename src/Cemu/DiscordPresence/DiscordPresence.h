#pragma once

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

private:
	class DiscordRPCLite* m_rpcClient = nullptr;
};
