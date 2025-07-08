#pragma once
#include "util/helpers/Semaphore.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "Cafe/TitleList/TitleId.h"
#include "util/helpers/ConcurrentQueue.h"
#include "config/NetworkSettings.h"

// forward declarations
namespace NAPI
{
	struct IDBEIconDataV0;
	struct AuthInfo;
}

namespace NCrypto
{
	class TMDParser;
}

struct DlMgrTitleReport
{
	enum class STATUS
	{
		INSTALLABLE, // not a package
		INSTALLABLE_UNFINISHED, // same as INSTALLABLE, but a previous unfinished package was detected
		INSTALLABLE_UPDATE, // same as INSTALLABLE, but an older version is already installed (used for DLC updates after purchasing more content)
		// below are packages
		QUEUED,
		PAUSED,
		INITIALIZING, // not active yet, downloading TMD
		CHECKING, // checking for previously downloaded files
		DOWNLOADING, // downloading content files
		VERIFYING, // verifying downloaded files
		INSTALLING,
		INSTALLED,
		HAS_ERROR
	};

	DlMgrTitleReport(STATUS status, uint64 titleId, uint16 version, std::string name, uint32 progress, uint32 progressMax, bool isPaused) : status(status), titleId(titleId), version(version), name(name), progress(progress), progressMax(progressMax), isPaused(isPaused) {}

	uint64 titleId;
	uint16 version;
	std::string name; // utf8
	STATUS status;
	uint32 progress;
	uint32 progressMax;
	bool isPaused;
	std::string errorMsg;
};

enum class DLMGR_STATUS_CODE
{
	UNINITIALIZED,
	CONNECTING,
	FAILED,
	CONNECTED
};

class DownloadManager
{
public:
	/* singleton */

	static DownloadManager* GetInstance(bool createIfNotExist = true)
	{
		static DownloadManager* s_instance = nullptr;
		if (s_instance)
			return s_instance;
		if (createIfNotExist)
			s_instance = new DownloadManager();
		return s_instance;
	}

	// login
	void connect(
		std::string_view nnidAccountName, 
		const std::array<uint8, 32>& passwordHash, 
		CafeConsoleRegion region,
		std::string_view country,
		uint32 deviceId,
		std::string_view serial,
		std::string_view deviceCertBase64);

	bool IsConnected() const;

private:
	/* connect / login */
	
	enum class CONNECT_STATE
	{
		UNINITIALIZED = 0, // connect() not called
		REQUESTED = 1, // connect() requested, but not being processed yet
		PROCESSING = 2, // processing login request
		COMPLETE = 3, // login complete / succeeded
		FAILED = 4, // failed to login
	};

	struct 
	{
		std::string cachefileName;
		std::string nnidAccountName;
		std::array<uint8, 32> passwordHash;
		std::string deviceCertBase64;
		CafeConsoleRegion region;
		std::string country;
		uint32 deviceId; // deviceId without platform (0x5<<32 for WiiU)
		std::string serial;
	}m_authInfo{};

	struct  
	{
		// auth info we have to request from the server
		std::string serviceAccountId; // internal account id (integer) provided by server when registering account (GetRegistrationInfo)
		std::string deviceToken;
	}m_iasToken{};

	std::atomic<CONNECT_STATE> m_connectState{ CONNECT_STATE::UNINITIALIZED };

	void _handle_connect();
	bool _connect_refreshIASAccountIdAndDeviceToken();
	bool _connect_queryAccountStatusAndServiceURLs();

	NetworkService GetDownloadMgrNetworkService();
	NAPI::AuthInfo GetAuthInfo(bool withIasToken);

	/* idbe cache */
public:
	void prepareIDBE(uint64 titleId);
	std::string getNameFromCachedIDBE(uint64 titleId);

private:
	NAPI::IDBEIconDataV0* getIDBE(uint64 titleId);

	/* ticket cache */
public:

	struct ETicketInfo
	{
		enum class SOURCE : uint8
		{
			ECS_TICKET = 0, // personalized ticket of owned title
			PUBLIC_TICKET = 1, // public ticket file (available as /CETK from NUS server or via SOAP GetSystemCommonETicket. The former is from Wii era while the latter is how Wii U requests public tickets?)
			// note: These ids are baked into the serialized cache format. Do not modify
		};

		ETicketInfo(SOURCE source, uint64 ticketId, uint32 ticketVersion, std::vector<uint8>& eTicket);
		ETicketInfo(SOURCE source, uint64 ticketId, uint32 ticketVersion, std::vector<uint8>& eTicket, std::vector<std::vector<uint8>>& eTicketCerts);
		void GetTitleKey(NCrypto::AesKey& key);

		SOURCE source;
		// tiv
		uint64 ticketId;
		uint32 ticketVersion;
		// titleInfo
		uint64 titleId{0}; // parsed from eTicket

		std::vector<uint8> eTicket;
		std::vector<std::vector<uint8>> eTicketCerts;
	};

	std::vector<ETicketInfo> m_ticketCache;

	struct UnfinishedDownload
	{
		UnfinishedDownload(uint64 titleId, uint16 titleVersion) : titleId(titleId), titleVersion(titleVersion) {};

		uint64 titleId;
		uint16 titleVersion;
	};

	std::vector<UnfinishedDownload> m_unfinishedDownloads;

	void loadTicketCache();
	void storeTicketCache();

	bool syncAccountTickets();
	bool syncSystemTitleTickets();
	bool syncUpdateTickets();
	bool syncTicketCache();
	void searchForIncompleteDownloads();

	void reportAvailableTitles();

private:
	ETicketInfo* findTicketByTicketId(uint64 ticketId)
	{
		for (auto& itr : m_ticketCache)
		{
			if (itr.ticketId == ticketId)
				return &itr;
		}
		return nullptr;
	}

	void deleteTicketByTicketId(uint64 ticketId)
	{
		auto itr = m_ticketCache.begin();
		while (itr != m_ticketCache.end())
		{
			if (itr->ticketId == ticketId)
			{
				m_ticketCache.erase(itr);
				return;
			}
			itr++;
		}
		cemu_assert_debug(false); // ticketId not found
	}

	ETicketInfo* findTicketByTitleIdAndVersion(uint64 titleId, uint16 version)
	{
		for (auto& itr : m_ticketCache)
		{
			if (itr.titleId == titleId && itr.ticketVersion == version)
				return &itr;
		}
		return nullptr;
	}

	ETicketInfo* findFirstTicketByTitleId(uint64 titleId)
	{
		for (auto& itr : m_ticketCache)
		{
			if (itr.titleId == titleId)
				return &itr;
		}
		return nullptr;
	}

	bool hasPartialDownload(uint64 titleId, uint16 titleVersion)
	{
		for (auto& itr : m_unfinishedDownloads)
		{
			if (itr.titleId == titleId && itr.titleVersion == titleVersion)
				return true;
		}
		return false;
	}

	void deleteUnfinishedDownloadRecord(uint64 titleId)
	{
		auto itr = m_unfinishedDownloads.begin();
		while (itr != m_unfinishedDownloads.end())
		{
			if (itr->titleId == titleId)
			{
				itr = m_unfinishedDownloads.erase(itr);
				continue;
			}
			itr++;
		}
	}

	/* packages / downloading */
	struct Package 
	{
		Package(uint64 titleId, uint16 version, std::span<uint8> ticketData) : titleId(titleId), version(version), eTicketData(ticketData.data(), ticketData.data() + ticketData.size()) 
		{
			NCrypto::ETicketParser eTicketParser;
			cemu_assert(eTicketParser.parse(ticketData.data(), ticketData.size()));
			eTicketParser.GetTitleKey(ticketKey);
		};

		~Package()
		{
			delete state.tmd;
		}

		enum class STATE
		{
			INITIAL,
			CHECKING,
			DOWNLOADING,
			VERIFYING,
			INSTALLING,
			INSTALLED // done
		};

		uint64 titleId;
		uint16 version;
		std::vector<uint8> eTicketData;
		NCrypto::AesKey ticketKey;

		// internal
		struct ContentFile
		{
			ContentFile(uint16 index, uint32 contentId, uint64 size, NCrypto::TMDParser::TMDContentFlags contentFlags, uint8 hash[32]) : index(index), contentId(contentId), size(size), contentFlags(contentFlags)
			{
				std::memcpy(contentHash, hash, 32);
				CalcPaddedSize();
			};
			const uint16 index;
			const uint32 contentId;
			const uint64 size;
			uint64 paddedSize; // includes padding forced by encryption/hashing (e.g. Nintendo Land update has one non-hashed content size of 0x8001, padded to 0x8010)
			const NCrypto::TMDParser::TMDContentFlags contentFlags;
			uint8 contentHash[32];

			enum class STATE
			{
				CHECK,
				DOWNLOAD,
				VERIFY,
				INSTALL,

				ENUM_COUNT, // for enum_array
			};

			STATE currentState{STATE::CHECK};
			bool isBeingProcessed{false}; // worked thread assigned and processing current state

			void finishProcessing(STATE newState)
			{
				currentState = newState;
				isBeingProcessed = false;
			};

			// progress tracking
			uint64 amountDownloaded{};

		private:
			void CalcPaddedSize()
			{
				if (HAS_FLAG(contentFlags, NCrypto::TMDParser::TMDContentFlags::FLAG_HASHED_CONTENT))
				{
					// pad to 0x10000 bytes
					paddedSize = (size + 0xFFFFull) & ~0xFFFFull;
				}
				else
				{
					// pad to 16 bytes
					paddedSize = (size + 0xFull) & ~0xFull;
				}
			}
		};

		struct
		{
			bool isActive{}; // actively downloading 
			bool isPaused{};
			STATE currentState{ STATE::INITIAL };
			// tmd
			bool isDownloadingTMD{};
			std::vector<uint8> tmdData;
			NCrypto::TMDParser* tmd{};
			// app/h3 tracking
			std::unordered_map<uint16, ContentFile> contentFiles;
			// progress of current operation
			uint32 progress{}; // for downloading: in 1/10th of a percent (0-1000), for installing: number of files
			uint32 progressMax{}; // maximum (downloading: unused, installing: total number of files)
			// installing
			bool isInstalling{};
			// error state
			bool hasError{};
			std::string errorMsg;
		}state;
	};

	std::recursive_mutex m_mutex;
	std::vector<Package*> m_packageList;

public:
	void initiateDownload(uint64 titleId, uint16 version);
	void pauseDownload(uint64 titleId, uint16 version);

private:
	Package* getPackage(uint64 titleId, uint16 version);
	fs::path getPackageDownloadPath(Package* package);
	fs::path getPackageInstallPath(Package* package);
	void updatePackage(Package* package);
	void checkPackagesState();

	void setPackageError(Package* package, std::string errorMsg);
	void reportPackageStatus(Package* package);
	void reportPackageProgress(Package* package, uint32 newProgress);

	void asyncPackageDownloadTMD(Package* package);
	void calcPackageDownloadProgress(Package* package);
	void asyncPackageDownloadContentFile(Package* package, uint16 index);
	void asyncPackageVerifyFile(Package* package, uint16 index, bool isCheckState);
	void asyncPackageInstall(Package* package);
	bool asyncPackageInstallRecursiveExtractFiles(Package* package, class FSTVolume* fstVolume, const std::string& sourcePath, const fs::path& destinationPath);

	/* callback interface */
public:
	void setUserData(void* ptr)
	{
		m_userData = ptr;
	}

	void* getUserData() const
	{
		return m_userData;
	}

	// register/unregister callbacks
	// setting valid callbacks will also trigger transfer of the entire title/package state and the current status message
	void registerCallbacks(
		void(*cbUpdateConnectStatus)(std::string statusText, DLMGR_STATUS_CODE statusCode),
		void(*cbAddDownloadableTitle)(const DlMgrTitleReport& titleInfo),
		void(*cbRemoveDownloadableTitle)(uint64 titleId, uint16 version)
	)
	{
		std::unique_lock<std::recursive_mutex> _l(m_mutex);
		m_cbUpdateConnectStatus = cbUpdateConnectStatus;
		m_cbAddDownloadableTitle = cbAddDownloadableTitle;
		m_cbRemoveDownloadableTitle = cbRemoveDownloadableTitle;
		// resend data
		if (m_cbUpdateConnectStatus || m_cbAddDownloadableTitle)
		{
			std::unique_lock<std::recursive_mutex> _l(m_mutex);
			setStatusMessage(m_statusMessage, m_statusCode);
			reportAvailableTitles();
			for (auto& p : m_packageList)
				reportPackageStatus(p);
		}
	}

	void setStatusMessage(std::string_view msg, DLMGR_STATUS_CODE statusCode)
	{
		m_statusMessage = msg;
		m_statusCode = statusCode;
		if (m_cbUpdateConnectStatus)
			m_cbUpdateConnectStatus(m_statusMessage, statusCode);
	}

	std::string m_statusMessage{};
	DLMGR_STATUS_CODE m_statusCode{ DLMGR_STATUS_CODE::UNINITIALIZED };

	bool hasActiveDownloads()
	{
		std::unique_lock<std::recursive_mutex> _l(m_mutex);
		for (auto& p : m_packageList)
		{
			if (p->state.isActive && !p->state.hasError &&
				(p->state.currentState != Package::STATE::INSTALLED))
			{
				return true;
			}
		}
		return false;
	}

	void reset()
	{
		std::unique_lock<std::recursive_mutex> _l(m_mutex);
		m_packageList.clear();
		m_statusCode = DLMGR_STATUS_CODE::UNINITIALIZED;
		m_statusMessage.clear();
	}

private:
	void(*m_cbUpdateConnectStatus)(std::string statusText, DLMGR_STATUS_CODE statusCode) { nullptr };
	void(*m_cbAddDownloadableTitle)(const DlMgrTitleReport& titleInfo);
	void(*m_cbRemoveDownloadableTitle)(uint64 titleId, uint16 version);
	void* m_userData{};

	/* title version list */
	bool m_hasTitleVersionList{};
	std::unordered_map<uint64, uint16> m_titleVersionList;

	void downloadTitleVersionList();
	bool getTitleLatestVersion(TitleId titleId, uint16& version);

	/* helper for building list of owned titles (is installed or has cached ticket) */
	struct TitleInstallState
	{
		TitleInstallState(DownloadManager* dlMgr, uint64 titleId);

		uint64 titleId;
		bool isInstalled;
		uint16 installedTitleVersion;
		uint16 installedTicketVersion{};

		bool operator<(const TitleInstallState& comp) const
		{
			return this->titleId < comp.titleId;
		}
	};

	struct TitleDownloadAvailableState : TitleInstallState
	{
		TitleDownloadAvailableState(DownloadManager* dlMgr, uint64 titleId);

		bool isUpdateAvailable; // update available for this specific titleId
		uint16 availableTitleVersion;
	};

	std::set<TitleInstallState> getOwnedTitleList();
	std::set<TitleDownloadAvailableState> getFullDownloadList();

	/* thread */
	void threadFunc();
	void runManager();
	void notifyManager();
	void queueManagerJob(const std::function<void()>& callback);

	std::atomic_bool m_threadLaunched{ false };
	CounterSemaphore m_queuedEvents;

	ConcurrentQueue<std::function<void()>> m_jobQueue;

	std::mutex m_updateList; // not needed?
};
