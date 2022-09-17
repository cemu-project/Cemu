#include "config/ActiveSettings.h"

#include "Cafe/OS/libs/nn_aoc/nn_aoc.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/Filesystem/fsc.h"
#include "Cafe/TitleList/TitleId.h"

#include "Cemu/ncrypto/ncrypto.h"
#include "Common/FileStream.h"

namespace nn
{
	namespace aoc
	{
		struct AOCTitle
		{
			uint64be titleId;
			uint32be groupId;
			uint16be titleVersion;
			char path[88];
			uint8 padding[2];
		};
		static_assert(sizeof(AOCTitle) == 0x68);

		enum class AOC_RESULT : uint32
		{
			ERROR_OK = 0,
		};

		uint32 AOC_CalculateWorkBufferSize(uint32 count)
		{
			count = std::min(count, (uint32)256);
			uint32 workBufferSize = 0x80 + count * 0x61;
			return workBufferSize;
		}

		struct AOCCacheEntry
		{
			AOCCacheEntry(uint64 titleId) : aocTitleId(titleId) {};

			uint64 aocTitleId;
			std::string GetPath()
			{
				return fmt::format("/vol/aoc{:016x}", aocTitleId);
			}
		};

		std::vector<AOCCacheEntry> sAocCache;
		bool sAocCacheGenerated = false;

		void generateAOCList()
		{
			if (sAocCacheGenerated)
				return;
			sAocCacheGenerated = true;

			sint32 fscStatus;
			FSCVirtualFile* volDirIterator = fsc_openDirIterator("/vol", &fscStatus);
			cemu_assert_debug(volDirIterator); // for valid titles /vol should always exist
			if (volDirIterator)
			{
				FSCDirEntry dirEntry;
				while (fsc_nextDir(volDirIterator, &dirEntry))
				{
					std::string_view dirName = dirEntry.GetPath();
					if(!dirEntry.isDirectory)
						continue;
					// check for pattern: aoc<titleId>
					if(dirName.size() != (3+16))
						continue;
					if(dirName[0] != 'a' ||
						dirName[1] != 'o' ||
						dirName[2] != 'c')
						continue;
					TitleId aocTitleId;
					if( !TitleIdParser::ParseFromStr(dirName.substr(3), aocTitleId) )
						continue;
					// add to list of known AOC
					sAocCache.emplace_back(aocTitleId);
				}
				fsc_close(volDirIterator);
			}
		}

		AOC_RESULT AOC_ListTitle(uint32be* titleCountOut, AOCTitle* titleList, uint32 maxCount, void* workBuffer, uint32 workBufferSize)
		{
			generateAOCList();
			for (uint32 i = 0; i < std::min(maxCount, (uint32)sAocCache.size()); i++)
			{
				titleList[i].titleId = sAocCache[i].aocTitleId;
				titleList[i].groupId = 0; // todo
				titleList[i].titleVersion = 0; // todo
				strcpy(titleList[i].path, sAocCache[i].GetPath().c_str());
			}
			*titleCountOut = std::min(maxCount, (uint32)sAocCache.size());
			return AOC_RESULT::ERROR_OK;
		}

		AOC_RESULT AOC_OpenTitle(char* pathOut, AOCTitle* aocTitleInfo, void* workBuffer, uint32 workBufferSize)
		{
			strcpy(pathOut, aocTitleInfo->path);
			return AOC_RESULT::ERROR_OK;
		}

		AOC_RESULT AOC_CloseTitle(void* ukn)
		{
			return AOC_RESULT::ERROR_OK;
		}

		AOC_RESULT AOC_GetPurchaseInfo(uint32be* purchaseBoolArrayOut, uint64 titleId, uint16be* entryIds, uint32 entryCount, void* workBuffer, uint32 workBufferSize)
		{
			// open ticket file
			// on an actual Wii U they get stored to SLC but the download manager places these in the code folder currently
			const auto ticketPath = ActiveSettings::GetMlcPath(L"usr/title/{:08x}/{:08x}/code/title.tik", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
			uint32 tikFileSize = 0;
			std::unique_ptr<FileStream> fileStream(FileStream::openFile2(ticketPath));
			std::vector<uint8> tikData;
			if (fileStream)
				fileStream->extract(tikData);
			if (tikData.size() > 0)
			{
				NCrypto::ETicketParser eTicket;
				if (eTicket.parse(tikData.data(), tikData.size()))
				{
					for (uint32 i = 0; i < entryCount; i++)
					{
						uint16 id = entryIds[i];
						if (eTicket.CheckRight(id))
							purchaseBoolArrayOut[i] = 1;
						else
							purchaseBoolArrayOut[i] = 0;
					}
					cemuLog_log(LogType::Force, "Using content rights from AOC title.tik");
					return AOC_RESULT::ERROR_OK;
				}
				else
				{
					cemuLog_log(LogType::Force, "Unable to parse AOC title.tik");
				}
			}

			// fallback: return true for all contentIds
			for (uint32 i = 0; i < entryCount; i++)
			{
				uint16 id = entryIds[i];
				purchaseBoolArrayOut[i] = 1;
			}

			return AOC_RESULT::ERROR_OK;
		}

		void Initialize()
		{
			cafeExportRegister("nn_aoc", AOC_CalculateWorkBufferSize, LogType::NN_AOC);
			cafeExportRegister("nn_aoc", AOC_ListTitle, LogType::NN_AOC);

			cafeExportRegister("nn_aoc", AOC_OpenTitle, LogType::NN_AOC);
			cafeExportRegister("nn_aoc", AOC_CloseTitle, LogType::NN_AOC);
			cafeExportRegister("nn_aoc", AOC_GetPurchaseInfo, LogType::NN_AOC);
		}
	}
}
