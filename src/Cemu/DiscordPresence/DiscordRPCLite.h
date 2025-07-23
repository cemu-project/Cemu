/* Standalone client for Discord RPC for the sole purpose of updating rich presence */

#pragma once
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>
#include <mutex>

struct DiscordLiteRichPresence
{
	std::string state;
	std::string details;
	int64_t startTimestamp;
	int64_t endTimestamp;
	std::string largeImageKey;
	std::string largeImageText;
	std::string smallImageKey;
	std::string smallImageText;
	std::string partyId;
	int partySize;
	int partyMax;
	int partyPrivacy;
	std::string matchSecret;
	std::string joinSecret;
	std::string spectateSecret;
	int8_t instance;
};

class DiscordRPCLite
{
  public:
	DiscordRPCLite(const char* applicationId);
	~DiscordRPCLite();

	void UpdateRichPresence(const DiscordLiteRichPresence& presence)
	{
		std::unique_lock _l(m_presenceMutex);
		m_presence = presence;
		m_richPresenceDirty = true;
	}

	void ClearRichPresence()
	{
		std::unique_lock _l(m_presenceMutex);
		m_presence = {};
		m_richPresenceDirty = false;
	}

  private:
	void WorkerThread(std::string applicationId);

	std::thread m_workerThread;
	std::atomic_bool m_shutdownSignal{false};
	// rich presence state
	std::mutex m_presenceMutex;
	std::atomic_bool m_richPresenceDirty{false};
	DiscordLiteRichPresence m_presence;
};