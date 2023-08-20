#include "iosu_nim.h"
#include "iosu_ioctl.h"
#include "iosu_act.h"
#include "iosu_mcp.h"
#include "util/crypto/aes128.h"
#include "curl/curl.h"
#include "openssl/bn.h"
#include "openssl/x509.h"
#include "openssl/ssl.h"
#include "util/helpers/helpers.h"
#include "Cemu/napi/napi.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "Cafe/CafeSystem.h"

namespace iosu
{
	namespace nim
	{
		struct NIMTitleLatestVersion
		{
			uint64 titleId;
			uint32 version;
		};

#define PACKAGE_TYPE_UPDATE		(1)

		struct NIMPackage
		{
			uint64 titleId;
			uint16 version;
			uint8 type;
		};

		struct
		{
			bool isInitialized;
			// version list
			sint32 latestVersion;
			char fqdn[256];
			std::vector<NIMTitleLatestVersion> titlesLatestVersion;
			// nim packages
			// note: Seems like scope.rpx expects the number of packages to never change after the initial GetNum call?
			std::vector<NIMPackage*> packages;
			bool packageListReady;
			bool backgroundThreadStarted;
		} g_nim = {};

		bool nim_CheckDownloadsDisabled()
		{
			// currently for the Wii U menu we disable NIM to speed up boot times
			uint64 tid = CafeSystem::GetForegroundTitleId();
			return tid == 0x0005001010040000 || tid == 0x0005001010040100 || tid == 0x0005001010040200;
		}

		bool nim_getLatestVersion()
		{
			g_nim.latestVersion = -1;
			NAPI::AuthInfo authInfo;
			authInfo.country = NCrypto::GetCountryAsString(Account::GetCurrentAccount().GetCountry());
			authInfo.region = NCrypto::SEEPROM_GetRegion();
			auto versionListVersionResult = NAPI::TAG_GetVersionListVersion(authInfo);
			if (!versionListVersionResult.isValid)
				return false;
			if (versionListVersionResult.fqdnURL.size() >= 256)
			{
				cemuLog_log(LogType::Force, "NIM: fqdn URL too long");
				return false;
			}
			g_nim.latestVersion = (sint32)versionListVersionResult.version;
			strcpy(g_nim.fqdn, versionListVersionResult.fqdnURL.c_str());
			return true;
		}

		bool nim_getVersionList()
		{
			g_nim.titlesLatestVersion.clear();

			NAPI::AuthInfo authInfo;
			authInfo.country = NCrypto::GetCountryAsString(Account::GetCurrentAccount().GetCountry());
			authInfo.region = NCrypto::SEEPROM_GetRegion();
			auto versionListVersionResult = NAPI::TAG_GetVersionListVersion(authInfo);
			if (!versionListVersionResult.isValid)
				return false;
			auto versionListResult = TAG_GetVersionList(authInfo, versionListVersionResult.fqdnURL, versionListVersionResult.version);
			if (!versionListResult.isValid)
				return false;
			for (auto& itr : versionListResult.titleVersionList)
			{
				NIMTitleLatestVersion titleLatestVersion;
				titleLatestVersion.titleId = itr.first;
				titleLatestVersion.version = itr.second;
				g_nim.titlesLatestVersion.push_back(titleLatestVersion);
			}
			return true;
		}

		NIMTitleLatestVersion* nim_findTitleLatestVersion(uint64 titleId)
		{
			for (auto& titleLatestVersion : g_nim.titlesLatestVersion)
			{
				if (titleLatestVersion.titleId == titleId)
					return &titleLatestVersion;
			}
			return nullptr;
		}

		void nim_buildDownloadList()
		{
			if(nim_CheckDownloadsDisabled())
			{
				cemuLog_logDebug(LogType::Force, "nim_buildDownloadList: Downloads are disabled for this title");
				g_nim.packages.clear();
				return;
			}

			sint32 titleCount = mcpGetTitleCount();
			MCPTitleInfo* titleList = (MCPTitleInfo*)malloc(titleCount * sizeof(MCPTitleInfo));
			memset(titleList, 0, titleCount * sizeof(MCPTitleInfo));

			uint32be titleCountBE = titleCount;
			if (mcpGetTitleList(titleList, titleCount * sizeof(MCPTitleInfo), &titleCountBE) != 0)
			{
				cemuLog_log(LogType::Force, "IOSU: nim failed to acquire title list");
				free(titleList);
				return;
			}
			titleCount = titleCountBE;
			// check for game updates
			for (sint32 i = 0; i < titleCount; i++)
			{
				if( titleList[i].titleIdHigh != 0x00050000 )
					continue;
				// find update title in title version list
				uint64 titleId = (0x0005000EULL << 32) | ((uint64)(uint32)titleList[i].titleIdLow);
				NIMTitleLatestVersion* latestVersionInfo = nim_findTitleLatestVersion(titleId);
				if(latestVersionInfo == nullptr)
					continue;
				// compare version
				if(latestVersionInfo->version <= (uint32)titleList[i].titleVersion )
					continue; // already on latest version
				// add to packages
				NIMPackage* nimPackage = (NIMPackage*)malloc(sizeof(NIMPackage));
				memset(nimPackage, 0, sizeof(NIMPackage));
				nimPackage->titleId = titleId;
				nimPackage->type = PACKAGE_TYPE_UPDATE;
				g_nim.packages.push_back(nimPackage);
			}
			// check for AOC/titles to download
			// todo
			free(titleList);
		}

		void nim_getPackagesInfo(uint64* titleIdList, sint32 count, titlePackageInfo_t* packageInfoList)
		{
			memset(packageInfoList, 0, sizeof(titlePackageInfo_t)*count);
			if(nim_CheckDownloadsDisabled())
				return;
			for (sint32 i = 0; i < count; i++)
			{
				uint64 titleId = _swapEndianU64(titleIdList[i]);
				titlePackageInfo_t* packageInfo = packageInfoList + i;

				packageInfo->titleId = _swapEndianU64(titleIdList[0]);

				packageInfo->ukn0C = 1; // update

				// pending
				packageInfo->ukn48 = 0; // with 0 there is no progress bar, with 3 there is a progress bar
				packageInfo->ukn40 = (5 << 20) | (0 << 27) | (1<<31) | (0x30000<<0);

				packageInfo->ukn0F = 1;

			}

		}

		struct idbeIconCacheEntry_t
		{
			void setIconData(NAPI::IDBEIconDataV0& newIconData)
			{
				iconData = newIconData;
				hasIconData = true;
			}

			void setNoIcon()
			{
				hasIconData = false;
				iconData = {};
			}

			uint64 titleId;
			uint32 lastRequestTime;
			bool hasIconData{ false };
			NAPI::IDBEIconDataV0 iconData{};
		};

		std::vector<idbeIconCacheEntry_t> idbeIconCache;

		void idbe_addIconToCache(uint64 titleId, NAPI::IDBEIconDataV0* iconData)
		{
			idbeIconCacheEntry_t newCacheEntry;
			newCacheEntry.titleId = titleId;
			newCacheEntry.lastRequestTime = GetTickCount();
			if (iconData)
				newCacheEntry.setIconData(*iconData);
			else
				newCacheEntry.setNoIcon();
			idbeIconCache.push_back(newCacheEntry);
		}

		sint32 nim_getIconDatabaseEntry(uint64 titleId, void* idbeIconOutput)
		{
			// if titleId is an update, replace it with gameId instead
			if ((uint32)(titleId >> 32) == 0x0005000EULL)
			{
				titleId &= ~0xF00000000ULL;
			}

			for (auto& entry : idbeIconCache)
			{
				if (entry.titleId == titleId)
				{
					if( entry.hasIconData )
						memcpy(idbeIconOutput, &entry.iconData, sizeof(NAPI::IDBEIconDataV0));
					else
						memset(idbeIconOutput, 0, sizeof(NAPI::IDBEIconDataV0));
					return 0;
				}
			}

			auto result = NAPI::IDBE_Request(titleId);
			if (!result)
			{
				memset(idbeIconOutput, 0, sizeof(NAPI::IDBEIconDataV0));
				cemuLog_log(LogType::Force, "NIM: Unable to download IDBE icon");
				return 0;
			}
			// add new cache entry
			idbe_addIconToCache(titleId, &*result);
			// return result
			memcpy(idbeIconOutput, &*result, sizeof(NAPI::IDBEIconDataV0));
			return 0;
		}

		void nim_backgroundThread()
		{
			while (iosuAct_isAccountDataLoaded() == false)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}

			if (nim_getLatestVersion())
			{
				if (nim_getVersionList())
				{
					nim_buildDownloadList();
				}
			}
			g_nim.packageListReady = true;
		}

		void iosuNim_waitUntilPackageListReady()
		{
			if (g_nim.backgroundThreadStarted == false)
			{
				cemuLog_log(LogType::Force, "IOSU: Starting nim background thread");
				std::thread t(nim_backgroundThread);
				t.detach();
				g_nim.backgroundThreadStarted = true;
			}
			while (g_nim.packageListReady == false)
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}


		void iosuNim_thread()
		{
			SetThreadName("iosuNim_thread");
			while (true)
			{
				uint32 returnValue = 0; // Ioctl return value
				ioQueueEntry_t* ioQueueEntry = iosuIoctl_getNextWithWait(IOS_DEVICE_NIM);
				if (ioQueueEntry->request == IOSU_NIM_REQUEST_CEMU)
				{
					iosuNimCemuRequest_t* nimCemuRequest = (iosuNimCemuRequest_t*)ioQueueEntry->bufferVectors[0].buffer.GetPtr();
					if (nimCemuRequest->requestCode == IOSU_NIM_GET_ICON_DATABASE_ENTRY)
					{
						nimCemuRequest->returnCode = nim_getIconDatabaseEntry(nimCemuRequest->titleId, nimCemuRequest->ptr.GetPtr());
					}
					else if (nimCemuRequest->requestCode == IOSU_NIM_GET_PACKAGE_COUNT)
					{
						iosuNim_waitUntilPackageListReady();
						nimCemuRequest->resultU32.u32 = (uint32)g_nim.packages.size();
						nimCemuRequest->returnCode = 0;
					}
					else if (nimCemuRequest->requestCode == IOSU_NIM_GET_PACKAGES_TITLEID)
					{
						iosuNim_waitUntilPackageListReady();
						uint32 maxCount = nimCemuRequest->maxCount;
						uint64* titleIdList = (uint64*)nimCemuRequest->ptr.GetPtr();
						uint32 count = 0;
						memset(titleIdList, 0, sizeof(uint64) * maxCount);
						for (auto& package : g_nim.packages)
						{
							titleIdList[count] = _swapEndianU64(package->titleId);
							count++;
							if (count >= maxCount)
								break;
						}
						nimCemuRequest->returnCode = 0;
					}
					else if (nimCemuRequest->requestCode == IOSU_NIM_GET_PACKAGES_INFO)
					{
						iosuNim_waitUntilPackageListReady();
						uint32 maxCount = nimCemuRequest->maxCount;
						uint64* titleIdList = (uint64*)nimCemuRequest->ptr.GetPtr();
						titlePackageInfo_t* packageInfo = (titlePackageInfo_t*)nimCemuRequest->ptr2.GetPtr();
						nim_getPackagesInfo(titleIdList, maxCount, packageInfo);
						nimCemuRequest->returnCode = 0;
					}
					else
						cemu_assert_unimplemented();
				}
				else
					cemu_assert_unimplemented();
				iosuIoctl_completeRequest(ioQueueEntry, returnValue);
			}
			return;
		}

		void Initialize()
		{
			if (g_nim.isInitialized)
				return;
			std::vector<idbeIconCacheEntry_t> idbeIconCache = std::vector<idbeIconCacheEntry_t>();
			g_nim.titlesLatestVersion = std::vector<NIMTitleLatestVersion>();
			g_nim.packages = std::vector<NIMPackage*>();
			g_nim.packageListReady = false;
			g_nim.backgroundThreadStarted = false;
			std::thread t2(iosuNim_thread);
			t2.detach();
			g_nim.isInitialized = true;
		}

	}
}

