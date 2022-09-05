#include "Cemu/Tools/DownloadManager/DownloadManager.h"

#include "Cafe/Account/Account.h"
#include "gui/CemuApp.h"
#include "util/crypto/md5.h"
#include "Cafe/TitleList/TitleId.h"
#include "Common/filestream.h"
#include "Cemu/FileCache/FileCache.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "config/ActiveSettings.h"
#include "util/ThreadPool/ThreadPool.h"
#include "util/helpers/enum_array.hpp"
#include "gui/MainWindow.h"

#include "Cafe/Filesystem/FST/FST.h"
#include "Cafe/TitleList/TitleList.h"

#include <cinttypes>
#include <charconv>
#include <curl/curl.h>
#include <pugixml.hpp>

#include "gui/helpers/wxHelpers.h"

#include "Cemu/napi/napi.h"
#include "util/helpers/Serializer.h"

FileCache* s_nupFileCache = nullptr;

/* version list */

void DownloadManager::downloadTitleVersionList()
{
	if (m_hasTitleVersionList)
		return;
	NAPI::AuthInfo authInfo;
	authInfo.accountId = m_authInfo.nnidAccountName;
	authInfo.passwordHash = m_authInfo.passwordHash;
	authInfo.deviceId = m_authInfo.deviceId;
	authInfo.serial = m_authInfo.serial;
	authInfo.country = m_authInfo.country;
	authInfo.region = m_authInfo.region;
	authInfo.deviceCertBase64 = m_authInfo.deviceCertBase64;
	auto versionListVersionResult = NAPI::TAG_GetVersionListVersion(authInfo);
	if (!versionListVersionResult.isValid)
		return;
	auto versionListResult = TAG_GetVersionList(authInfo, versionListVersionResult.fqdnURL, versionListVersionResult.version);
	if (!versionListResult.isValid)
		return;
	m_titleVersionList = versionListResult.titleVersionList;
	m_hasTitleVersionList = true;
}

// grab latest version from TAG version list. Returns false if titleId is not found in the list
bool DownloadManager::getTitleLatestVersion(TitleId titleId, uint16& version)
{
	auto titleVersionAvailability = m_titleVersionList.find(titleId);
	if (titleVersionAvailability == m_titleVersionList.end())
		return false;
	version = titleVersionAvailability->second;
	return true;
}

/* helper method to generate list of owned base titles */

DownloadManager::TitleInstallState::TitleInstallState(DownloadManager* dlMgr, uint64 titleId) : titleId(titleId)
{
	uint16 vers;	
	if (CafeTitleList::HasTitle(titleId, vers))
	{
		isInstalled = true;
		installedTitleVersion = vers;
	}
	else
	{
		isInstalled = false;
		installedTitleVersion = 0;
	}
	installedTicketVersion = 0;
	// for DLC we also retrieve the ticket version from the installed title.tik
	// todo - avoid file reads here due to stalling the UI thread
	if (TitleIdParser(titleId).GetType() == TitleIdParser::TITLE_TYPE::AOC)
	{
		const auto ticketPath = ActiveSettings::GetMlcPath(L"usr/title/{:08x}/{:08x}/code/title.tik", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		uint32 tikFileSize = 0;
		FileStream* fileStream = FileStream::openFile2(ticketPath);
		if (fileStream)
		{
			std::vector<uint8> tikData;
			fileStream->extract(tikData);
			NCrypto::ETicketParser eTicket;
			if (eTicket.parse(tikData.data(), tikData.size()))
				installedTicketVersion = eTicket.GetTicketVersion();
			delete fileStream;
		}
	}
}

DownloadManager::TitleDownloadAvailableState::TitleDownloadAvailableState(DownloadManager* dlMgr, uint64 titleId) : TitleInstallState(dlMgr, titleId)
{
	// get latest available version of this title (from the TAG version list)
	uint16 vers;
	if (dlMgr->getTitleLatestVersion(titleId, vers))
	{
		isUpdateAvailable = true;
		availableTitleVersion = vers;
	}
	else
	{
		isUpdateAvailable = false;
		availableTitleVersion = 0;
	}
};

std::set<DownloadManager::TitleInstallState> DownloadManager::getOwnedTitleList()
{
	std::set<DownloadManager::TitleInstallState> ownedTitleList;
	// add installed games and DLC
	std::vector<TitleId> installedTitleIds = CafeTitleList::GetAllTitleIds();
	for (auto& itr : installedTitleIds)
	{
		TitleIdParser titleIdParser(itr);
		auto titleType = titleIdParser.GetType();
		if (titleType == TitleIdParser::TITLE_TYPE::BASE_TITLE ||
			titleType == TitleIdParser::TITLE_TYPE::BASE_TITLE_DEMO ||
			titleType == TitleIdParser::TITLE_TYPE::SYSTEM_OVERLAY_TITLE)
		{
			ownedTitleList.emplace(this, itr);
		}
	}
	// add ticket cache
	for (auto& itr : m_ticketCache)
	{
		TitleIdParser titleIdParser(itr.titleId);
		auto titleType = titleIdParser.GetType();
		if (titleType == TitleIdParser::TITLE_TYPE::BASE_TITLE ||
			titleType == TitleIdParser::TITLE_TYPE::BASE_TITLE_DEMO ||
			titleType == TitleIdParser::TITLE_TYPE::SYSTEM_OVERLAY_TITLE ||
			titleType == TitleIdParser::TITLE_TYPE::AOC)
		{
			ownedTitleList.emplace(this, itr.titleId);
		}
	}

	return ownedTitleList;
}

std::set<DownloadManager::TitleDownloadAvailableState> DownloadManager::getFullDownloadList()
{
	std::set<DownloadManager::TitleDownloadAvailableState> fullList;
	// get list of owned titles
	std::set<DownloadManager::TitleInstallState> ownedTitleList = getOwnedTitleList();
	// add each owned title, but also check for separate updates if available
	for (auto& itr : ownedTitleList)
	{
		TitleIdParser titleIdParser(itr.titleId);
		fullList.emplace(this, itr.titleId);
		if (titleIdParser.CanHaveSeparateUpdateTitleId())
		{
			uint64 updateTitleId = titleIdParser.GetSeparateUpdateTitleId();
			uint16 tempVers;
			if (getTitleLatestVersion(updateTitleId, tempVers))
				fullList.emplace(this, updateTitleId);
		}
	}
	return fullList;
}

/* connect */

struct StoredTokenInfo : public SerializerHelper
{
	bool serializeImpl(MemStreamWriter& streamWriter)
	{
		streamWriter.writeBE<uint8>(0);
		streamWriter.writeBE<std::string>(accountId);
		streamWriter.writeBE<std::string>(deviceToken);
		return true;
	}

	bool deserializeImpl(MemStreamReader& streamReader)
	{
		auto version = streamReader.readBE<uint8>();
		if (version != 0)
			return false;
		accountId = streamReader.readBE<std::string>();
		deviceToken = streamReader.readBE<std::string>();
		return !streamReader.hasError();
	}

public:
	std::string accountId;
	std::string deviceToken;
};

bool DownloadManager::_connect_refreshIASAccountIdAndDeviceToken()
{
	NAPI::AuthInfo authInfo;
	authInfo.accountId = m_authInfo.nnidAccountName;
	authInfo.passwordHash = m_authInfo.passwordHash;
	authInfo.deviceId = m_authInfo.deviceId;
	authInfo.serial = m_authInfo.serial;
	authInfo.country = m_authInfo.country;
	authInfo.region = m_authInfo.region;
	authInfo.deviceCertBase64 = m_authInfo.deviceCertBase64;

	// query IAS/ECS account id and device token (if not cached)
	auto rChallenge = NAPI::IAS_GetChallenge(authInfo);
	if (rChallenge.apiError != NAPI_RESULT::SUCCESS)
		return false;
	auto rRegistrationInfo = NAPI::IAS_GetRegistrationInfo_QueryInfo(authInfo, rChallenge.challenge);
	if (rRegistrationInfo.apiError != NAPI_RESULT::SUCCESS)
		return false;

	m_iasToken.serviceAccountId = rRegistrationInfo.accountId;
	m_iasToken.deviceToken = rRegistrationInfo.deviceToken;
	// store to cache
	StoredTokenInfo storedTokenInfo;
	storedTokenInfo.accountId = rRegistrationInfo.accountId;
	storedTokenInfo.deviceToken = rRegistrationInfo.deviceToken;
	std::vector<uint8> serializedData;
	if (!storedTokenInfo.serialize(serializedData))
		return false;
	s_nupFileCache->AddFileAsync({ fmt::format("{}/token_info", m_authInfo.nnidAccountName) }, serializedData.data(), serializedData.size());
	return true;
}

bool DownloadManager::_connect_queryAccountStatusAndServiceURLs()
{
	NAPI::AuthInfo authInfo;
	authInfo.accountId = m_authInfo.nnidAccountName;
	authInfo.passwordHash = m_authInfo.passwordHash;
	authInfo.deviceId = m_authInfo.deviceId;
	authInfo.serial = m_authInfo.serial;
	authInfo.country = m_authInfo.country;
	authInfo.region = m_authInfo.region;
	authInfo.deviceCertBase64 = m_authInfo.deviceCertBase64;

	authInfo.IASToken.accountId = m_iasToken.serviceAccountId;
	authInfo.IASToken.deviceToken = m_iasToken.deviceToken;

	NAPI::NAPI_ECSGetAccountStatus_Result accountStatusResult = NAPI::ECS_GetAccountStatus(authInfo);
	if (accountStatusResult.apiError != NAPI_RESULT::SUCCESS)
	{
		cemuLog_log(LogType::Force, "ECS - Failed to query account status (error: {0} {1})", accountStatusResult.apiError, accountStatusResult.serviceError);
		return false;
	}
	if (accountStatusResult.accountStatus == NAPI::NAPI_ECSGetAccountStatus_Result::AccountStatus::UNREGISTERED)
	{
		cemuLog_log(LogType::Force, fmt::format("ECS - Account is not registered"));
		return false;
	}

	return true;
}

// constructor for ticket cache entry
DownloadManager::ETicketInfo::ETicketInfo(SOURCE source, uint64 ticketId, uint32 ticketVersion, std::vector<uint8>& eTicket)
	: source(source), ticketId(ticketId), ticketVersion(ticketVersion), 
	eTicket(eTicket)
{
	NCrypto::ETicketParser eTicketParser;
	if (!eTicketParser.parse(eTicket.data(), eTicket.size()))
	{
		titleId = (uint64)-1;
		return;
	}

	cemu_assert_debug(!eTicketParser.IsPersonalized()); // ticket should have been depersonalized already
	titleId = eTicketParser.GetTitleId();
	ticketVersion = eTicketParser.GetTicketVersion();
	cemu_assert_debug(ticketId == eTicketParser.GetTicketId());
	cemu_assert_debug(ticketVersion == eTicketParser.GetTicketVersion());
}

DownloadManager::ETicketInfo::ETicketInfo(SOURCE source, uint64 ticketId, uint32 ticketVersion, std::vector<uint8>& eTicket, std::vector<std::vector<uint8>>& eTicketCerts)
	: ETicketInfo(source, ticketId, ticketVersion, eTicket)
{
	this->eTicketCerts = eTicketCerts;
}

void DownloadManager::ETicketInfo::GetTitleKey(NCrypto::AesKey& key)
{
	NCrypto::ETicketParser eTicketParser;
	cemu_assert_debug(eTicketParser.parse(eTicket.data(), eTicket.size()));
	eTicketParser.GetTitleKey(key);
}

void DownloadManager::loadTicketCache()
{
	m_ticketCache.clear();
	cemu_assert_debug(m_ticketCache.empty());
	std::vector<uint8> ticketCacheBlob;	
	if (!s_nupFileCache->GetFile({ fmt::format("{}/eticket_cache", m_authInfo.nnidAccountName) }, ticketCacheBlob))
		return;
	MemStreamReader memReader(ticketCacheBlob.data(), ticketCacheBlob.size());
	uint8 version = memReader.readBE<uint8>();
	if (version != 1)
		return; // unsupported version
	uint32 numTickets = memReader.readBE<uint32>();
	for (uint32 i = 0; i < numTickets; i++)
	{
		if (memReader.hasError())
		{
			m_ticketCache.clear();
			return;
		}
		ETicketInfo::SOURCE source = (ETicketInfo::SOURCE)memReader.readBE<uint8>();
		if (source != ETicketInfo::SOURCE::ECS_TICKET && source != ETicketInfo::SOURCE::PUBLIC_TICKET)
		{
			m_ticketCache.clear();
			return;
		}
		uint64 ticketId = memReader.readBE<uint64>();
		uint64 ticketVersion = memReader.readBE<uint32>();
		std::vector<uint8> eTicketData = memReader.readPODVector<uint8>();
		std::vector<std::vector<uint8>> eTicketCerts;
		uint8 certCount = memReader.readBE<uint8>();
		for (uint32 c = 0; c < certCount; c++)
			eTicketCerts.emplace_back(memReader.readPODVector<uint8>());
		if (memReader.hasError())
		{
			m_ticketCache.clear();
			return;
		}
		m_ticketCache.emplace_back(source, ticketId, ticketVersion, eTicketData, eTicketCerts);
	}
}

void DownloadManager::storeTicketCache()
{
	MemStreamWriter memWriter(1024*32);
	memWriter.writeBE<uint8>(1); // version
	memWriter.writeBE<uint32>((uint32)m_ticketCache.size());
	for (auto& eTicket : m_ticketCache)
	{
		memWriter.writeBE<uint8>((uint8)eTicket.source);
		memWriter.writeBE<uint64>(eTicket.ticketId);
		memWriter.writeBE<uint32>(eTicket.ticketVersion);
		memWriter.writePODVector(eTicket.eTicket);
		memWriter.writeBE<uint8>(eTicket.eTicketCerts.size());
		for (auto& cert : eTicket.eTicketCerts)
			memWriter.writePODVector(cert);
	}
	auto serializedBlob = memWriter.getResult();
	s_nupFileCache->AddFileAsync({ fmt::format("{}/eticket_cache", m_authInfo.nnidAccountName) }, serializedBlob.data(), serializedBlob.size());
}

bool DownloadManager::syncAccountTickets()
{
	NAPI::AuthInfo authInfo;
	authInfo.accountId = m_authInfo.nnidAccountName;
	authInfo.passwordHash = m_authInfo.passwordHash;
	authInfo.deviceId = m_authInfo.deviceId;
	authInfo.serial = m_authInfo.serial;
	authInfo.country = m_authInfo.country;
	authInfo.region = m_authInfo.region;
	authInfo.deviceCertBase64 = m_authInfo.deviceCertBase64;

	authInfo.IASToken.accountId = m_iasToken.serviceAccountId;
	authInfo.IASToken.deviceToken = m_iasToken.deviceToken;

	// query TIV list from server
	NAPI::NAPI_ECSAccountListETicketIds_Result resultTicketIds = NAPI::ECS_AccountListETicketIds(authInfo);
	if (!resultTicketIds.isValid())
		return false;

	// download uncached tickets
	size_t count = resultTicketIds.tivs.size();
	size_t index = 0;
	for (auto& tiv : resultTicketIds.tivs)
	{
		index++;
		std::string msg = _("Downloading account ticket").ToStdString();
		msg.append(fmt::format(" {0}/{1}", index, count));
		setStatusMessage(msg, DLMGR_STATUS_CODE::CONNECTING);
		// skip if already cached
		ETicketInfo* cachedTicket = findTicketByTicketId(tiv.ticketId);
		if (cachedTicket)
		{
			if(cachedTicket->ticketVersion == tiv.ticketVersion)
				continue;
			// ticket version mismatch, redownload
			deleteTicketByTicketId(tiv.ticketId);
		}
		// get ECS ticket
		auto resultETickets = NAPI::ECS_AccountGetETickets(authInfo, tiv.ticketId);
		if (!resultETickets.isValid())
		{
			cemuLog_log(LogType::Force, "SyncTicketCache: Account ETicket invalid");
			continue;
		}
		// verify ticket integrity
		NCrypto::ETicketParser eTicketParser;
		if (!eTicketParser.parse(resultETickets.eTickets.data(), resultETickets.eTickets.size()))
			continue;
		uint64 titleId = eTicketParser.GetTitleId();
		cemu_assert_debug(eTicketParser.GetTicketId() == tiv.ticketId);
		cemu_assert_debug(eTicketParser.GetTicketVersion() == tiv.ticketVersion);

		// depersonalize the ticket
		if (eTicketParser.IsPersonalized())
		{
			NCrypto::ECCPrivKey privKey = NCrypto::ECCPrivKey::getDeviceCertPrivateKey();
			if (!eTicketParser.Depersonalize(resultETickets.eTickets.data(), resultETickets.eTickets.size(), m_authInfo.deviceId, privKey))
			{
				cemuLog_log(LogType::Force, "DownloadManager: Failed to depersonalize ticket");
				continue;
			}
			// reparse
			cemu_assert(eTicketParser.parse(resultETickets.eTickets.data(), resultETickets.eTickets.size()));
		}
		else
		{
			cemuLog_log(LogType::Force, "DownloadManager: Unexpected result. ECS ticket not personalized");
			continue;
		}

		ETicketInfo eTicket(ETicketInfo::SOURCE::ECS_TICKET, tiv.ticketId, tiv.ticketVersion, resultETickets.eTickets, resultETickets.certs);
		m_ticketCache.emplace_back(eTicket);
	}
	return true;
}

bool DownloadManager::syncSystemTitleTickets()
{
	setStatusMessage(std::string(_("Downloading system tickets...")), DLMGR_STATUS_CODE::CONNECTING);
	// todo - add GetAuth() function
	NAPI::AuthInfo authInfo;
	authInfo.accountId = m_authInfo.nnidAccountName;
	authInfo.passwordHash = m_authInfo.passwordHash;
	authInfo.deviceId = m_authInfo.deviceId;
	authInfo.serial = m_authInfo.serial;
	authInfo.country = m_authInfo.country;
	authInfo.region = m_authInfo.region;
	authInfo.deviceCertBase64 = m_authInfo.deviceCertBase64;

	authInfo.IASToken.accountId = m_iasToken.serviceAccountId;
	authInfo.IASToken.deviceToken = m_iasToken.deviceToken;

	auto querySystemTitleTicket = [&](uint64 titleId) -> void
	{
		// check if cached already
		// todo - how do we know which version to query? System titles seem to use hashes?
		if (findFirstTicketByTitleId(titleId))
			return;
		// request ticket
		auto resultCommonETicket = NAPI::NUS_GetSystemCommonETicket(authInfo, titleId);
		if (!resultCommonETicket.isValid())
			return;
		// parse and validate ticket
		NCrypto::ETicketParser eTicketParser;
		if (!eTicketParser.parse(resultCommonETicket.eTicket.data(), resultCommonETicket.eTicket.size()))
			return;
		if (eTicketParser.GetTitleId() != titleId)
			return;
		if (eTicketParser.IsPersonalized())
			return;
		// add to eTicket cache
		ETicketInfo eTicket(ETicketInfo::SOURCE::PUBLIC_TICKET, eTicketParser.GetTicketId(), eTicketParser.GetTicketVersion(), resultCommonETicket.eTicket, resultCommonETicket.certs);
		m_ticketCache.emplace_back(eTicket);
	};

	if (m_authInfo.region == CafeConsoleRegion::EUR)
	{
		querySystemTitleTicket(0x000500301001420A); // eShop
		querySystemTitleTicket(0x000500301001520A); // Friend List
		querySystemTitleTicket(0x000500301001220A); // Internet browser
	}
	else if (m_authInfo.region == CafeConsoleRegion::USA)
	{
		querySystemTitleTicket(0x000500301001410A); // eShop
		querySystemTitleTicket(0x000500301001510A); // Friend List
		querySystemTitleTicket(0x000500301001210A); // Internet browser
	}
	else if (m_authInfo.region == CafeConsoleRegion::JPN)
	{
		querySystemTitleTicket(0x000500301001400A); // eShop
		querySystemTitleTicket(0x000500301001500A); // Friend List
		querySystemTitleTicket(0x000500301001200A); // Internet browser
	}

	return true;
}

// build list of updates for which either an installed game exists or the base title ticket is cached
bool DownloadManager::syncUpdateTickets()
{
	setStatusMessage(std::string(_("Retrieving update information...")), DLMGR_STATUS_CODE::CONNECTING);
	// download update version list
	downloadTitleVersionList();
	if (!m_hasTitleVersionList)
		return false;
	std::set<DownloadManager::TitleDownloadAvailableState> downloadList = getFullDownloadList();
	// count updates
	size_t numUpdates = 0;
	for (auto& itr : downloadList)
	{
		TitleIdParser titleIdParser(itr.titleId);
		if (titleIdParser.GetType() == TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE)
			numUpdates++;
	}
	// get tickets for all the updates
	size_t updateIndex = 0;
	for (auto& itr : downloadList)
	{
		TitleIdParser titleIdParser(itr.titleId);
		if (titleIdParser.GetType() != TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE)
			continue;

		std::string msg = _("Downloading ticket").ToStdString();
		msg.append(fmt::format(" {0}/{1}", updateIndex, numUpdates));
		updateIndex++;
		setStatusMessage(msg, DLMGR_STATUS_CODE::CONNECTING);

		if (!itr.isUpdateAvailable)
			continue;

		// skip if already cached
		if (findTicketByTitleIdAndVersion(itr.titleId, itr.availableTitleVersion))
			continue;

		NAPI::AuthInfo dummyAuth;
		auto cetkResult = NAPI::CCS_GetCETK(dummyAuth, itr.titleId, itr.availableTitleVersion);
		if (!cetkResult.isValid)
			continue;
		NCrypto::ETicketParser ticketParser;
		if (!ticketParser.parse(cetkResult.cetkData.data(), cetkResult.cetkData.size()))
			continue;
		uint64 ticketId = ticketParser.GetTicketId();
		uint64 ticketTitleId = ticketParser.GetTitleId();
		uint16 ticketTitleVersion = ticketParser.GetTicketVersion();
		uint16 ticketVersion = ticketTitleVersion;
		if (ticketTitleId != itr.titleId)
			continue;
		if (ticketTitleVersion != itr.availableTitleVersion)
		{
			cemuLog_log(LogType::Force, "Ticket for title update has a mismatching version");
			continue;
		}
		// add to eTicket cache
		ETicketInfo eTicket(ETicketInfo::SOURCE::PUBLIC_TICKET, ticketId, ticketVersion, cetkResult.cetkData);
		m_ticketCache.emplace_back(eTicket);
	}
	return true;
}

// synchronize ticket cache with server and request uncached ticket data
bool DownloadManager::syncTicketCache()
{
	if (!syncAccountTickets())
		return false;
	syncSystemTitleTickets();
	syncUpdateTickets();
	storeTicketCache();

	// make sure IDBE's are loaded into memory for all eTickets (potential downloads)
	// this will only download them if they aren't already in the on-disk cache
	size_t count = m_ticketCache.size();
	size_t index = 0;
	for (auto& ticketInfo : m_ticketCache)
	{
		index++;
		std::string msg = _("Downloading meta data").ToStdString();
		msg.append(fmt::format(" {0}/{1}", index, count));
		setStatusMessage(msg, DLMGR_STATUS_CODE::CONNECTING);
		prepareIDBE(ticketInfo.titleId);
	}
	setStatusMessage(std::string(_("Connected. Right click entries in the list to start downloading")), DLMGR_STATUS_CODE::CONNECTED);
	return true;
}

void DownloadManager::searchForIncompleteDownloads()
{
	const fs::path packagePath = ActiveSettings::GetMlcPath("usr/packages/title/");
	if (!fs::exists(packagePath))
		return;
	
	for (auto& p : fs::directory_iterator(packagePath))
	{
		uint64 titleId;
		uint32 version;
		std::string name = p.path().filename().generic_string();
		if( sscanf(name.c_str(), "cemu_%" PRIx64 "_v%u", &titleId, &version) != 2)
			continue;
		std::unique_lock<std::recursive_mutex> _l(m_mutex);
		for (auto& itr : m_ticketCache)
			m_unfinishedDownloads.emplace_back(titleId, version);
	}
}

void DownloadManager::reportAvailableTitles()
{
	if (!m_cbAddDownloadableTitle)
		return;

	std::set<DownloadManager::TitleDownloadAvailableState> downloadList = getFullDownloadList();

	for (auto& itr : downloadList)
	{
		TitleIdParser titleIdParser(itr.titleId);
		TitleIdParser::TITLE_TYPE titleType = titleIdParser.GetType();
		if (titleType == TitleIdParser::TITLE_TYPE::BASE_TITLE ||
			titleType == TitleIdParser::TITLE_TYPE::BASE_TITLE_DEMO ||
			titleType == TitleIdParser::TITLE_TYPE::SYSTEM_OVERLAY_TITLE ||
			titleType == TitleIdParser::TITLE_TYPE::BASE_TITLE_UPDATE ||
			titleType == TitleIdParser::TITLE_TYPE::AOC)
		{
			// show entries only if we were able to retrieve the ticket
			if (itr.isInstalled)
			{
				if (!findFirstTicketByTitleId(itr.titleId))
					continue;
			}

			DlMgrTitleReport::STATUS status = DlMgrTitleReport::STATUS::INSTALLABLE;
			bool aocHasUpdate = false;
			if (itr.isInstalled && titleType == TitleIdParser::TITLE_TYPE::AOC)
			{
				ETicketInfo* eTicketInfo = findFirstTicketByTitleId(itr.titleId);
				if (eTicketInfo && eTicketInfo->ticketVersion > itr.installedTicketVersion)
					aocHasUpdate = true;
			}

			if (itr.isInstalled && itr.installedTitleVersion >= itr.availableTitleVersion && aocHasUpdate == false)
			{
				status = DlMgrTitleReport::STATUS::INSTALLED;
			}
			if (status == DlMgrTitleReport::STATUS::INSTALLABLE)
			{
				if (hasPartialDownload(itr.titleId, itr.availableTitleVersion))
					status = DlMgrTitleReport::STATUS::INSTALLABLE_UNFINISHED;
				if (aocHasUpdate)
					status = DlMgrTitleReport::STATUS::INSTALLABLE_UPDATE;
			}
			DlMgrTitleReport titleInfo(status, itr.titleId, itr.availableTitleVersion, getNameFromCachedIDBE(itr.titleId), 0, 0, false);
			m_cbAddDownloadableTitle(titleInfo);
		}
		else
		{
			cemu_assert_debug(false); // unsupported title type
		}
	}
}

/* connection logic */

void DownloadManager::_handle_connect()
{
	m_connectState.store(CONNECT_STATE::PROCESSING);
	
	// reset login state
	m_iasToken.serviceAccountId.clear();
	m_iasToken.deviceToken.clear();
	setStatusMessage(std::string(_("Logging in..")), DLMGR_STATUS_CODE::CONNECTING);
	// retrieve ECS AccountId + DeviceToken from cache
	if (s_nupFileCache)
	{
		std::vector<uint8> serializationBlob;
		if (s_nupFileCache->GetFile({ fmt::format("{}/token_info", m_authInfo.nnidAccountName) }, serializationBlob))
		{
			StoredTokenInfo storedTokenInfo;
			if (storedTokenInfo.deserialize(serializationBlob))
			{
				m_iasToken.serviceAccountId = storedTokenInfo.accountId;
				m_iasToken.deviceToken = storedTokenInfo.deviceToken;
			}
		}
	}
	// .. or request AccountId and DeviceToken if not cached
	if (m_iasToken.serviceAccountId.empty() || m_iasToken.deviceToken.empty())
	{
		if (!_connect_refreshIASAccountIdAndDeviceToken())
		{
			forceLog_printf("Failed to request IAS token");
			cemu_assert_debug(false);
			m_connectState.store(CONNECT_STATE::FAILED);
			setStatusMessage(std::string(_("Login failed. Outdated or incomplete online files?")), DLMGR_STATUS_CODE::FAILED);
			return;
		}
	}
	// get EC account status and service urls
	if (!_connect_queryAccountStatusAndServiceURLs())
	{
		m_connectState.store(CONNECT_STATE::FAILED);
		setStatusMessage(std::string(_("Failed to query account status. Invalid account information?")), DLMGR_STATUS_CODE::FAILED);
		return;
	}
	// load ticket cache and sync
	setStatusMessage(std::string(_("Updating ticket cache")), DLMGR_STATUS_CODE::CONNECTING);
	loadTicketCache();
	if (!syncTicketCache())
	{
		m_connectState.store(CONNECT_STATE::FAILED);
		setStatusMessage(std::string(_("Failed to request tickets (invalid NNID?)")), DLMGR_STATUS_CODE::FAILED);
		return;
	}
	searchForIncompleteDownloads();

	// notify about all available downloadable titles
	reportAvailableTitles();

	// print ticket info
	m_connectState.store(CONNECT_STATE::COMPLETE);
}

void DownloadManager::connect(
	std::string_view nnidAccountName,
	const std::array<uint8, 32>& passwordHash,
	CafeConsoleRegion region,
	std::string_view country,
	uint32 deviceId,
	std::string_view serial,
	std::string_view deviceCertBase64)
{
	if (nnidAccountName.empty())
	{
		m_connectState.store(CONNECT_STATE::FAILED);
		setStatusMessage(std::string(_("This account is not linked with an NNID")), DLMGR_STATUS_CODE::FAILED);
		return;
	}
	runManager();
	m_authInfo.nnidAccountName = nnidAccountName;
	m_authInfo.passwordHash = passwordHash;
	if (std::all_of(m_authInfo.passwordHash.begin(), m_authInfo.passwordHash.end(), [](uint8 v) { return v == 0; }))
	{
		cemuLog_log(LogType::Force, "DLMgr: Invalid password hash");
		m_connectState.store(CONNECT_STATE::FAILED);
		setStatusMessage(std::string(_("Failed. Account does not have password set")), DLMGR_STATUS_CODE::FAILED);
		return;
	}
	m_authInfo.region = region;
	m_authInfo.country = country;
	m_authInfo.deviceCertBase64 = deviceCertBase64;
	m_authInfo.deviceId = deviceId;
	m_authInfo.serial = serial;
	m_connectState.store(CONNECT_STATE::REQUESTED);
	notifyManager();
	queueManagerJob([this]() {_handle_connect(); });
}

bool DownloadManager::IsConnected() const
{
	return m_connectState.load() != CONNECT_STATE::UNINITIALIZED;
}

/* package / downloading */

// start/resume/retry download
void DownloadManager::initiateDownload(uint64 titleId, uint16 version)
{
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	Package* package = getPackage(titleId, version);
	if (package)
	{
		// remove pause state
		package->state.isPaused = false;
		// already exists, erase error state
		if (package->state.hasError)
		{
			package->state.hasError = false;
			reportPackageStatus(package);
		}
		checkPackagesState();
		return;
	}
	// find matching eTicket and get key
	std::vector<uint8>* ticketData = nullptr;
	for (auto& ticket : m_ticketCache)
	{
		if (ticket.titleId == titleId )//&& ticket.version == version)
		{
			ticketData = &ticket.eTicket;
			break;
		}
	}
	if (!ticketData)
		return;

	package = new Package(titleId, version, *ticketData);
	m_packageList.emplace_back(package);
	reportPackageStatus(package);
	checkPackagesState(); // will start downloading this package if none already active
}

void DownloadManager::pauseDownload(uint64 titleId, uint16 version)
{
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	Package* package = getPackage(titleId, version);
	if (!package || !package->state.isActive)
		return;
	package->state.isPaused = true;
	package->state.isActive = false;
	reportPackageStatus(package);
	checkPackagesState();
}

DownloadManager::Package* DownloadManager::getPackage(uint64 titleId, uint16 version)
{
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	auto itr = std::find_if(m_packageList.begin(), m_packageList.end(), [titleId, version](const Package* v) { return v->titleId == titleId && v->version == version; });
	if (itr == m_packageList.end())
		return nullptr;
	return *itr;
}

fs::path DownloadManager::getPackageDownloadPath(Package* package)
{
	return ActiveSettings::GetMlcPath(fmt::format("usr/packages/title/cemu_{:016x}_v{}/", package->titleId, package->version));
}

fs::path DownloadManager::getPackageInstallPath(Package* package)
{
	TitleIdParser tParser(package->titleId);

	const char* titleBasePath = "usr/title/";
	if(tParser.IsSystemTitle())
		titleBasePath = "sys/title/";
	return ActiveSettings::GetMlcPath(fmt::format("{}{:08x}/{:08x}/", titleBasePath, (uint32)(package->titleId>>32), (uint32)package->titleId));
}

// called when a package becomes active (queued to downloading) or when any of it's async download operations finishes
// initiates new async download, decrypt or install tasks
void DownloadManager::updatePackage(Package* package)
{
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	if (!package->state.isActive || package->state.isPaused || package->state.hasError)
		return;
	// do we have the TMD downloaded yet?
	if (!package->state.tmd)
	{
		if (!package->state.isDownloadingTMD)
		{
			package->state.isDownloadingTMD = true;
			ThreadPool::FireAndForget(&DownloadManager::asyncPackageDownloadTMD, this, package);
		}
		return;
	}

	using ContentState = Package::ContentFile::STATE;
	// count state totals
	struct ContentCountInfo
	{
		uint32 total{};
		uint32 processing{};
	};
	enum_array<Package::ContentFile::STATE, ContentCountInfo> contentCountTable;
	for (auto& itr : package->state.contentFiles)
	{
		auto state = itr.second.currentState;
		contentCountTable[state].total++;
		if(itr.second.isBeingProcessed)
			contentCountTable[state].processing++;
	}

	// utility method to grab next inactive content entry with a specific state
	auto getFirstInactiveContentByState = [&](ContentState state) -> Package::ContentFile*
	{
		auto itr = std::find_if(package->state.contentFiles.begin(), package->state.contentFiles.end(), [state](const auto& contentFile) {return contentFile.second.currentState == state && !contentFile.second.isBeingProcessed; });
		if (itr == package->state.contentFiles.end())
			return nullptr;
		return &(itr->second);
	};

	/************* content check phase *************/
	if (package->state.currentState == Package::STATE::INITIAL)
	{
		package->state.currentState = Package::STATE::CHECKING;
		package->state.progress = 0;
		package->state.progressMax = (uint32)package->state.contentFiles.size();
		reportPackageStatus(package);
	}
	while (contentCountTable[ContentState::CHECK].total > 0 && contentCountTable[ContentState::CHECK].processing < 2)
	{
		Package::ContentFile* contentPtr = getFirstInactiveContentByState(ContentState::CHECK);
		if (!contentPtr)
			break;
		contentPtr->isBeingProcessed = true;
		ThreadPool::FireAndForget(&DownloadManager::asyncPackageVerifyFile, this, package, contentPtr->index, true);
		contentCountTable[ContentState::CHECK].processing++;
	}
	if (contentCountTable[ContentState::CHECK].total > 0)
		return; // dont proceed to next phase until done

	/************* content download phase *************/
	while (contentCountTable[ContentState::DOWNLOAD].total > 0 && contentCountTable[ContentState::DOWNLOAD].processing < 2)
	{
		if (package->state.currentState == Package::STATE::CHECKING || package->state.currentState == Package::STATE::VERIFYING)
		{
			package->state.currentState = Package::STATE::DOWNLOADING;
			package->state.progress = 0;
			package->state.progressMax = 0;
			reportPackageStatus(package);
		}
		// download next file if there aren't 2 active downloads already
		Package::ContentFile* contentPtr = getFirstInactiveContentByState(ContentState::DOWNLOAD);
		if (!contentPtr)
			break;
		contentPtr->isBeingProcessed = true;
		ThreadPool::FireAndForget(&DownloadManager::asyncPackageDownloadContentFile, this, package, contentPtr->index);
		contentCountTable[ContentState::DOWNLOAD].processing++;
	}
	if (contentCountTable[ContentState::DOWNLOAD].total > 0)
		return;

	/************* content verification phase *************/
	if (package->state.currentState != Package::STATE::VERIFYING)
	{
		package->state.currentState = Package::STATE::VERIFYING;
		package->state.progress = 0;
		package->state.progressMax = (uint32)package->state.contentFiles.size();
		reportPackageStatus(package);
	}

	while (contentCountTable[ContentState::VERIFY].total > 0 && contentCountTable[ContentState::VERIFY].processing < 2)
	{
		Package::ContentFile* contentPtr = getFirstInactiveContentByState(ContentState::VERIFY);
		if (!contentPtr)
			break;
		contentPtr->isBeingProcessed = true;
		ThreadPool::FireAndForget(&DownloadManager::asyncPackageVerifyFile, this, package, contentPtr->index, true);
		contentCountTable[ContentState::VERIFY].processing++;
	}
	if (contentCountTable[ContentState::VERIFY].total > 0)
		return;

	/************* installing phase *************/
	if (!package->state.isInstalling)
	{
		if (package->state.currentState != Package::STATE::INSTALLING)
		{
			package->state.currentState = Package::STATE::INSTALLING;
			package->state.progress = 0;
			package->state.progressMax = 0;
			reportPackageStatus(package);
		}
		package->state.isInstalling = true;
		ThreadPool::FireAndForget(&DownloadManager::asyncPackageInstall, this, package);
	}
}

// checks for new packages to download if none are currently active
void DownloadManager::checkPackagesState()
{
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	bool hasActive = false;
	hasActive = std::find_if(m_packageList.begin(), m_packageList.end(),
		[](const Package* p) { return p->state.isActive; }) != m_packageList.end();
	if (!hasActive)
	{
		// start new download
		auto it = std::find_if(m_packageList.begin(), m_packageList.end(),
			[](const Package* p) { return !p->state.isActive && !p->state.hasError && !p->state.isPaused && p->state.currentState != Package::STATE::INSTALLED; });
		if (it != m_packageList.end())
		{
			Package* startedPackage = *it;
			startedPackage->state.isActive = true;
			updatePackage(startedPackage);
			reportPackageStatus(startedPackage);
		}
	}
}

void DownloadManager::setPackageError(Package* package, std::string errorMsg)
{
	package->state.isActive = false;
	if (package->state.hasError)
		return; // dont overwrite already set error message
	package->state.hasError = true;
	package->state.errorMsg = errorMsg;
	reportPackageStatus(package);
}

void DownloadManager::reportPackageStatus(Package* package)
{
	if (!m_cbAddDownloadableTitle)
		return;
	m_mutex.lock();
	DlMgrTitleReport::STATUS status = DlMgrTitleReport::STATUS::INITIALIZING;
	if (package->state.hasError)
	{
		status = DlMgrTitleReport::STATUS::HAS_ERROR;
	}
	else if (package->state.currentState == Package::STATE::INSTALLED)
		status = DlMgrTitleReport::STATUS::INSTALLED;
	else if (!package->state.isActive)
	{
		if (package->state.tmd)
			status = DlMgrTitleReport::STATUS::PAUSED;
		else
			status = DlMgrTitleReport::STATUS::QUEUED;
	}
	else if (package->state.tmd)
	{
		if (package->state.currentState == Package::STATE::CHECKING)
			status = DlMgrTitleReport::STATUS::CHECKING;
		else if (package->state.currentState == Package::STATE::VERIFYING)
			status = DlMgrTitleReport::STATUS::VERIFYING;
		else if (package->state.currentState == Package::STATE::INSTALLING)
			status = DlMgrTitleReport::STATUS::INSTALLING;
		else if (package->state.currentState == Package::STATE::INSTALLED)
			status = DlMgrTitleReport::STATUS::INSTALLED;
		else
			status = DlMgrTitleReport::STATUS::DOWNLOADING;
	}

	DlMgrTitleReport reportInfo(status, package->titleId, package->version, getNameFromCachedIDBE(package->titleId), package->state.progress, package->state.progressMax, package->state.isPaused);
	if (package->state.hasError)
		reportInfo.errorMsg = package->state.errorMsg;

	m_mutex.unlock();
	m_cbAddDownloadableTitle(reportInfo);
}

void DownloadManager::reportPackageProgress(Package* package, uint32 currentProgress)
{
	// todo - cooldown timer to avoid spamming too many events

	package->state.progress = currentProgress;
	reportPackageStatus(package);
}

void DownloadManager::asyncPackageDownloadTMD(Package* package)
{
	NAPI::AuthInfo authInfo;
	authInfo.accountId = m_authInfo.nnidAccountName;
	authInfo.passwordHash = m_authInfo.passwordHash;
	authInfo.deviceId = m_authInfo.deviceId;
	authInfo.serial = m_authInfo.serial;
	authInfo.country = m_authInfo.country;
	authInfo.region = m_authInfo.region;
	authInfo.deviceCertBase64 = m_authInfo.deviceCertBase64;
	authInfo.IASToken.accountId = m_iasToken.serviceAccountId;
	authInfo.IASToken.deviceToken = m_iasToken.deviceToken;

	TitleIdParser titleIdParser(package->titleId);
	NAPI::NAPI_CCSGetTMD_Result tmdResult;
	if (titleIdParser.GetType() == TitleIdParser::TITLE_TYPE::AOC)
	{
		// for AOC we always download the latest TMD
		// is there a way to get the version beforehand? It doesn't seem to be stored in either the .tik file or the update version list
		tmdResult = CCS_GetTMD(authInfo, package->titleId);
	}
	else
	{
		tmdResult = NAPI::CCS_GetTMD(authInfo, package->titleId, package->version);
	}	
	if (!tmdResult.isValid)
	{
		// failed, try to get latest TMD instead
		tmdResult = CCS_GetTMD(authInfo, package->titleId);
	}

	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	if (!tmdResult.isValid)
	{
		setPackageError(package, from_wxString(_("TMD download failed")));
		package->state.isDownloadingTMD = false;
		return;
	}
	_l.unlock();
	// parse
	NCrypto::TMDParser tmdParser;
	if (!tmdParser.parse(tmdResult.tmdData.data(), tmdResult.tmdData.size()))
	{
		setPackageError(package, from_wxString(_("Invalid TMD")));
		package->state.isDownloadingTMD = false;
		return;
	}
	// set TMD
	_l.lock();
	package->state.tmdData = tmdResult.tmdData;
	package->state.tmd = new NCrypto::TMDParser(tmdParser);
	package->state.isDownloadingTMD = false;
	// prepare list of content files
	package->state.contentFiles.clear();
	for (auto& itr : package->state.tmd->GetContentList())
	{
		if (package->state.contentFiles.find(itr.index) != package->state.contentFiles.end())
		{
			cemu_assert_debug(false);
			continue;
		}
		package->state.contentFiles.emplace(std::piecewise_construct, std::forward_as_tuple(itr.index), std::forward_as_tuple(itr.index, itr.contentId, itr.size, itr.contentFlags, itr.hash32));
	}
	// create folder
	auto dir = getPackageDownloadPath(package);
	fs::create_directories(dir);
	// continue with downloading
	reportPackageStatus(package);
	updatePackage(package);
}

void DownloadManager::calcPackageDownloadProgress(Package* package)
{
	if (package->state.currentState == Package::STATE::DOWNLOADING)
	{
		uint64 totalSize = 0;
		uint64 totalDownloaded = 0;
		for (auto& itr : package->state.contentFiles)
		{
			totalSize += itr.second.paddedSize;
			if (itr.second.currentState == Package::ContentFile::STATE::INSTALL || itr.second.currentState == Package::ContentFile::STATE::VERIFY)
				totalDownloaded += itr.second.paddedSize; // already downloaded, add full size
			else
				totalDownloaded += itr.second.amountDownloaded;
		}

		uint32 pct10 = (uint32)(totalDownloaded * 1000ull / totalSize);
		if (package->state.progress != pct10)
		{
			package->state.progress = pct10;
			package->state.progressMax = 1000;
			reportPackageProgress(package, package->state.progress);
		}
	}
}

void DownloadManager::asyncPackageDownloadContentFile(Package* package, uint16 index)
{
	// get titleId, contentId and file path
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	uint64 titleId = package->titleId;
	auto contentFileItr = package->state.contentFiles.find(index);
	cemu_assert(contentFileItr != package->state.contentFiles.end());
	uint32 contentId = contentFileItr->second.contentId;
	contentFileItr->second.amountDownloaded = 0;
	auto packageDownloadPath = getPackageDownloadPath(package);
	_l.unlock();

	// download h3 hash file (.h3) if flag 0x0002 is set (-> we are using the TMD to verify the hash of the content files)
	//auto h3Result = NAPI::CCS_GetContentH3File(titleId, contentId);
	//auto h3Result = NAPI::CCS_GetContentH3File(titleId, contentId);
	
	//if (!h3Result.isValid)
	//{
	//	setPackageError(package, "Download failed (h3)");
	//	return;
	//}
	//filePathStr = (packageDownloadPath / fmt::format("{:08x}.h3", index)).generic_u8string();
	//auto h3File = FileStream::createFile(filePathStr);
	//if (!h3File)
	//{
	//	setPackageError(package, "Cannot create file");
	//	return;
	//}
	//if (h3File->writeData(h3Result.tmdData.data(), h3Result.tmdData.size()) != h3Result.tmdData.size())
	//{
	//	setPackageError(package, "Cannot write file (h3). Disk full?");
	//	return;
	//}
	//delete h3File;

	// streamed download of content file (.app)
	// prepare callback parameter struct
	struct CallbackInfo 
	{
		DownloadManager* downloadMgr;
		Package* package;
		Package::ContentFile* contentFile;
		std::vector<uint8> receiveBuffer;
		FileStream* fileOutput;

		static bool writeCallback(void* userData, const void* ptr, size_t len, bool isLast)
		{
			CallbackInfo* callbackInfo = (CallbackInfo*)userData;
			// append bytes to buffer
			callbackInfo->receiveBuffer.insert(callbackInfo->receiveBuffer.end(), (const uint8*)ptr, (const uint8*)ptr + len);
			// flush cache to file if it exceeds 128KiB or if this is the final callback
			if (callbackInfo->receiveBuffer.size() >= (128 * 1024) || (isLast && !callbackInfo->receiveBuffer.empty()))
			{
				size_t bytesWritten = callbackInfo->receiveBuffer.size();
				if (callbackInfo->fileOutput->writeData(callbackInfo->receiveBuffer.data(), callbackInfo->receiveBuffer.size()) != (uint32)callbackInfo->receiveBuffer.size())
				{
					callbackInfo->downloadMgr->setPackageError(callbackInfo->package, from_wxString(_("Cannot write file. Disk full?")));
					return false;
				}
				callbackInfo->receiveBuffer.clear();
				if (bytesWritten > 0)
				{
					callbackInfo->downloadMgr->m_mutex.lock();
					callbackInfo->contentFile->amountDownloaded += bytesWritten;
					callbackInfo->downloadMgr->calcPackageDownloadProgress(callbackInfo->package);
					callbackInfo->downloadMgr->m_mutex.unlock();
				}
			}
			return true;
		}
	}callbackInfoData{};
	callbackInfoData.downloadMgr = this;
	callbackInfoData.package = package;
	callbackInfoData.contentFile = &contentFileItr->second;
	callbackInfoData.fileOutput = FileStream::createFile2(packageDownloadPath / fmt::format("{:08x}.app", contentId));
	if (!callbackInfoData.fileOutput)
	{
		setPackageError(package, from_wxString(_("Cannot create file")));
		return;
	}
	if (!NAPI::CCS_GetContentFile(titleId, contentId, CallbackInfo::writeCallback, &callbackInfoData))
	{
		setPackageError(package, from_wxString(_("Download failed")));
		delete callbackInfoData.fileOutput;
		return;
	}
	delete callbackInfoData.fileOutput;
	callbackInfoData.fileOutput = nullptr;

	// mark file as downloaded by requesting verify state
	_l.lock();
	contentFileItr->second.finishProcessing(Package::ContentFile::STATE::VERIFY);
	_l.unlock();
	// start next task
	updatePackage(package);
}

void DownloadManager::asyncPackageVerifyFile(Package* package, uint16 index, bool isCheckState)
{
	uint8 tmdContentHash[32];
	std::string filePathStr;
	// get titleId, contentId and file path
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	uint64 titleId = package->titleId;
	auto contentFileItr = package->state.contentFiles.find(index);
	cemu_assert(contentFileItr != package->state.contentFiles.end());
	uint16 contentIndex = contentFileItr->second.index;
	uint32 contentId = contentFileItr->second.contentId;
	uint64 contentSize = contentFileItr->second.size;
	uint64 contentPaddedSize = contentFileItr->second.paddedSize;
	auto contentFlags = contentFileItr->second.contentFlags;
	std::memcpy(tmdContentHash, contentFileItr->second.contentHash, 32);
	auto packageDownloadPath = getPackageDownloadPath(package);
	_l.unlock();
	NCrypto::AesKey ecsTicketKey = package->ticketKey;

	Package::ContentFile::STATE newStateOnError = Package::ContentFile::STATE::DOWNLOAD;
	Package::ContentFile::STATE newStateOnSuccess = Package::ContentFile::STATE::INSTALL;

	// verify file
	std::unique_ptr<FileStream> fileStream(FileStream::openFile2(packageDownloadPath / fmt::format("{:08x}.app", contentId)));
	if (!fileStream)
	{
		_l.lock();
		contentFileItr->second.finishProcessing(newStateOnError);
		if (!isCheckState)
			setPackageError(package, "Missing file during verification");
		_l.unlock();
		updatePackage(package);
		return;
	}

	bool isSHA1 = HAS_FLAG(contentFlags, NCrypto::TMDParser::TMDContentFlags::FLAG_SHA1);

	bool isValid = false;
	if (HAS_FLAG(contentFlags, NCrypto::TMDParser::TMDContentFlags::FLAG_HASHED_CONTENT))
		isValid = FSTVerifier::VerifyHashedContentFile(fileStream.get(), &ecsTicketKey, contentIndex, contentSize, contentPaddedSize, isSHA1, tmdContentHash);
	else
		isValid = FSTVerifier::VerifyContentFile(fileStream.get(), &ecsTicketKey, contentIndex, contentSize, contentPaddedSize, isSHA1, tmdContentHash);

	if (!isValid)
	{
		_l.lock();
		contentFileItr->second.finishProcessing(newStateOnError);
		if (!isCheckState)
			setPackageError(package, "Verification failed");
		_l.unlock();
		updatePackage(package);
		return;
	}
	// file verified successfully
	_l.lock();
	contentFileItr->second.finishProcessing(newStateOnSuccess);
	reportPackageProgress(package, package->state.progress + 1);
	_l.unlock();
	// start next task
	updatePackage(package);
}

bool DownloadManager::asyncPackageInstallRecursiveExtractFiles(Package* package, FSTVolume* fstVolume, const std::string& sourcePath, const fs::path& destinationPath)
{
	std::error_code ec;
	fs::create_directories(destinationPath, ec); // we dont check the error because it is OS/implementation specific (on Windows this returns ec=0 with false when directory already exists)
	cemu_assert_debug(sourcePath.back() == '/');
	FSTDirectoryIterator dirItr;
	if (!fstVolume->OpenDirectoryIterator(sourcePath, dirItr))
	{
		std::unique_lock<std::recursive_mutex> _l(m_mutex);
		setPackageError(package, "Internal error");
		return false;
	}

	FSTFileHandle itr;
	while (fstVolume->Next(dirItr, itr))
	{
		std::string_view nodeName = fstVolume->GetName(itr);
		if(nodeName.empty() || boost::equals(nodeName, ".") || boost::equals(nodeName, "..") || boost::contains(nodeName, "/") || boost::contains(nodeName, "\\"))
			continue;
		std::string sourceFilePath = sourcePath;
		sourceFilePath.append(nodeName);
		fs::path nodeDestinationPath = destinationPath;
		nodeDestinationPath.append(nodeName.data(), nodeName.data() + nodeName.size());
		if (fstVolume->IsDirectory(itr))
		{
			if (fstVolume->HasLinkFlag(itr))
			{
				// delete link directories
				fs::remove_all(nodeDestinationPath, ec);
			}
			else
			{
				// iterate
				sourceFilePath.push_back('/');
				asyncPackageInstallRecursiveExtractFiles(package, fstVolume, sourceFilePath, nodeDestinationPath);
			}
		}
		else if (fstVolume->IsFile(itr))
		{
			if (fstVolume->HasLinkFlag(itr))
			{
				// delete link files
				fs::remove_all(nodeDestinationPath, ec);
			}
			else
			{
				// extract
				std::vector<uint8> buffer(64 * 1024);
				FileStream* fileOut = FileStream::createFile2(nodeDestinationPath);
				if (!fileOut)
				{
					setPackageError(package, "Failed to create file");
					return false;
				}
				uint32 fileSize = fstVolume->GetFileSize(itr);
				uint32 currentPos = 0;
				while (currentPos < fileSize)
				{
					uint32 numBytesToTransfer = std::min(fileSize - currentPos, (uint32)buffer.size());
					if (fstVolume->ReadFile(itr, currentPos, numBytesToTransfer, buffer.data()) != numBytesToTransfer)
					{
						setPackageError(package, "Failed to extract data");
						return false;
					}
					if (fileOut->writeData(buffer.data(), numBytesToTransfer) != numBytesToTransfer)
					{
						setPackageError(package, "Failed to write to file. Disk full?");
						return false;
					}
					currentPos += numBytesToTransfer;
				}
				delete fileOut;
			}
			// advance progress
			std::unique_lock<std::recursive_mutex> _l(m_mutex);
			reportPackageProgress(package, package->state.progress + 1);
		}
		else
		{
			cemu_assert_debug(false); // unknown node type
		}
	}
	return true;
}

void DownloadManager::asyncPackageInstall(Package* package)
{
	std::unique_lock<std::recursive_mutex> _l(m_mutex);
	auto packageDownloadPath = getPackageDownloadPath(package);
	fs::path installPath = getPackageInstallPath(package);
	_l.unlock();
	// store title.tmd
	std::unique_ptr<FileStream> fileStream(FileStream::createFile2(packageDownloadPath / "title.tmd"));
	if (!fileStream || fileStream->writeData(package->state.tmdData.data(), package->state.tmdData.size()) != package->state.tmdData.size())
	{
		_l.lock();
		setPackageError(package, "Failed to write title.tmd");
		package->state.isInstalling = false;
		return;
	}
	fileStream.reset();
	// store title.tik
	fileStream.reset(FileStream::createFile2(packageDownloadPath / "title.tik"));
	if (!fileStream || fileStream->writeData(package->eTicketData.data(), package->eTicketData.size()) != package->eTicketData.size())
	{
		_l.lock();
		setPackageError(package, "Failed to write title.tik");
		package->state.isInstalling = false;
		return;
	}
	fileStream.reset();
	// for AOC titles we also 'install' the ticket by copying it next to the code/content/meta folders
	// on an actual Wii U the ticket gets installed to SLC but currently we only emulate MLC
	if (TitleIdParser(package->titleId).GetType() == TitleIdParser::TITLE_TYPE::AOC)
	{
		std::error_code ec;
		fs::create_directories(installPath, ec);
		fs::create_directories(installPath / "code/", ec);
		fileStream.reset(FileStream::createFile2(installPath / "code/title.tik"));
		if (!fileStream || fileStream->writeData(package->eTicketData.data(), package->eTicketData.size()) != package->eTicketData.size())
		{
			_l.lock();
			setPackageError(package, "Failed to install title.tik");
			package->state.isInstalling = false;
			return;
		}
		fileStream.reset();
	}
	// open app FST
	FSTVolume* fst = FSTVolume::OpenFromContentFolder(packageDownloadPath);
	if (!fst)
	{
		_l.lock();
		setPackageError(package, "Failed to extract content");
		package->state.isInstalling = false;
		return;
	}
	// count number of files for progress tracking
	package->state.progressMax = fst->GetFileCount();
	package->state.progress = 0;
	// extract code/content/meta folders into installation directory
	if (!asyncPackageInstallRecursiveExtractFiles(package, fst, "code/", installPath / "code"))
	{
		_l.lock();
		setPackageError(package, "Failed to extract code folder");
		package->state.isInstalling = false;
		return;
	}
	if (!asyncPackageInstallRecursiveExtractFiles(package, fst, "content/", installPath / "content"))
	{
		_l.lock();
		setPackageError(package, "Failed to extract content folder");
		package->state.isInstalling = false;
		return;
	}
	if (!asyncPackageInstallRecursiveExtractFiles(package, fst, "meta/", installPath / "meta"))
	{
		_l.lock();
		setPackageError(package, "Failed to extract meta folder");
		package->state.isInstalling = false;
		return;
	}
	delete fst;
	// delete package folder
	std::error_code ec;
	fs::remove_all(packageDownloadPath, ec);
	// mark as complete
	_l.lock();
	package->state.currentState = Package::STATE::INSTALLED;
	package->state.isInstalling = false;
	package->state.isActive = false;
	CafeTitleList::AddTitleFromPath(installPath);
	reportPackageStatus(package);
	checkPackagesState();
	// lastly request game list to be refreshed
	MainWindow::RequestGameListRefresh();
	return;
}

/* IDBE cache */

std::unordered_map<uint64, NAPI::IDBEIconDataV0*> s_idbeCache;
std::mutex s_idbeCacheMutex;

// load IDBE from disk or server into memory cache
// stalls while reading disk/downloading
void DownloadManager::prepareIDBE(uint64 titleId)
{
	auto hasInCache = [](uint64 titleId) -> bool
	{
		s_idbeCacheMutex.lock();
		bool hasCached = s_idbeCache.find(titleId) != s_idbeCache.end();
		s_idbeCacheMutex.unlock();
		return hasCached;
	};

	auto addToCache = [](uint64 titleId, NAPI::IDBEIconDataV0* iconData) -> void
	{
		NAPI::IDBEIconDataV0* iconInstance = new NAPI::IDBEIconDataV0();
		*iconInstance = *iconData;
		s_idbeCacheMutex.lock();
		if (!s_idbeCache.try_emplace(titleId, iconInstance).second)
			delete iconInstance;
		s_idbeCacheMutex.unlock();
	};
	if (hasInCache(titleId))
		return;
	// try to load from disk cache
	std::vector<uint8> idbeFile;
	if (s_nupFileCache->GetFile({ fmt::format("idbe/{0:016x}", titleId) }, idbeFile) && idbeFile.size() == sizeof(NAPI::IDBEIconDataV0))
		return addToCache(titleId, (NAPI::IDBEIconDataV0*)(idbeFile.data()));
	// not cached, query from server
	std::optional<NAPI::IDBEIconDataV0> iconData = NAPI::IDBE_Request(titleId);
	if (!iconData)
		return;
	s_nupFileCache->AddFileAsync({ fmt::format("idbe/{0:016x}", titleId) }, (uint8*)&(*iconData), sizeof(NAPI::IDBEIconDataV0));
	addToCache(titleId, &*iconData);
}

std::string DownloadManager::getNameFromCachedIDBE(uint64 titleId)
{
	// workaround for Friend List not having an IDBE
	if (titleId == 0x000500301001500A ||
		titleId == 0x000500301001510A ||
		titleId == 0x000500301001520A)
	{
		return "Friend List";
	}

	std::unique_lock<std::mutex> _l(s_idbeCacheMutex);
	NAPI::IDBEIconDataV0* iconData = getIDBE(titleId);
	if (iconData)
		return iconData->GetLanguageStrings(GetConfig().console_language).GetGameNameUTF8();
	return fmt::format("Title {0:016x}", titleId);
}

// returns IDBE by titleId (or nullptr if not in cache)
// assumes s_idbeCacheMutex is held by caller
NAPI::IDBEIconDataV0* DownloadManager::getIDBE(uint64 titleId)
{
	auto it = s_idbeCache.find(titleId);
	if (it == s_idbeCache.end())
		return nullptr;
	return it->second;
}

/* package manager / downloading */

void DownloadManager::threadFunc()
{
	while (true)
	{
		auto cb = m_jobQueue.pop();
		cb();
	}
}

// init download manager worker thread and cache
void DownloadManager::runManager()
{
	bool prevBool = false;
	if (!m_threadLaunched.compare_exchange_weak(prevBool, true))
		return;
	// open cache
	auto cacheFilePath = ActiveSettings::GetMlcPath("usr/save/system/nim/nup/");
	fs::create_directories(cacheFilePath);
	cacheFilePath /= "cemu_cache.dat";
	s_nupFileCache = FileCache::Open(cacheFilePath.generic_wstring(), true);
	// launch worker thread
	std::thread t(&DownloadManager::threadFunc, this);
	t.detach();
}

// let manager known there is a new event that needs processing
void DownloadManager::notifyManager()
{
	m_queuedEvents.increment();
}

void DownloadManager::queueManagerJob(const std::function<void()>& callback)
{
	m_jobQueue.push(callback);
}
