#include "iosu_pdm.h"
#include "config/ActiveSettings.h"
#include "Common/FileStream.h"
#include "util/helpers/Semaphore.h"

#if BOOST_OS_LINUX
// using chrono::year_month_date and other features require a relatively recent stdlibc++
// to avoid upping the required version we use the STL reference implementation for now
#include "Common/unix/date.h"
namespace chrono_d = date;
#else
namespace chrono_d = std::chrono;
#endif


namespace iosu
{
	namespace pdm
	{
		std::mutex sDiaryLock;

		fs::path GetPDFile(const char* filename)
		{
			// todo - support for per-account tracking
			return ActiveSettings::GetMlcPath(fmt::format("usr/save/system/pdm/80000001/{}", filename));
		}

		void MakeDirectory()
		{
			fs::path path = GetPDFile(".");
			std::error_code ec;
			fs::create_directories(path, ec);
		}

		chrono_d::year_month_day GetDateFromDayIndex(uint16 dayIndex)
		{
			chrono_d::sys_days startDateDay(chrono_d::year(2000) / chrono_d::January / chrono_d::day(1));
			chrono_d::sys_days lastPlayedDay = startDateDay + chrono_d::days(dayIndex);
			return chrono_d::year_month_day(lastPlayedDay);
		}

		uint16 GetTodaysDayIndex()
		{
			chrono_d::sys_days startDateDay(chrono_d::year(2000) / chrono_d::January / chrono_d::day(1));
			chrono_d::sys_days currentDateDay = chrono_d::floor<chrono_d::days>(std::chrono::system_clock::now());
			return (uint16)(currentDateDay - startDateDay).count();
		}

		struct PlayStatsEntry 
		{
			uint32be titleIdHigh;
			uint32be titleIdLow;
			uint32be totalMinutesPlayed;
			uint16be numTimesLaunched;
			uint16be firstLaunchDayIndex; // first time this title was launched
			uint16be mostRecentDayIndex; // last time this title was played
			uint16be ukn12; // maybe just padding?
		};

		static_assert(sizeof(PlayStatsEntry) == 0x14);

		struct
		{
			FileStream* fs{};
			uint32be numEntries;
			PlayStatsEntry entry[NUM_PLAY_STATS_ENTRIES];
		}PlayStats;

		void CreatePlaystats()
		{
			PlayStats.fs = FileStream::createFile2(GetPDFile("PlayStats.dat"));
			if (!PlayStats.fs)
			{
				cemuLog_log(LogType::Force, "Unable to open or create PlayStats.dat");
				return;
			}
			uint32be entryCount = 0;
			PlayStats.fs->writeData(&entryCount, sizeof(uint32be));
			PlayStats.fs->writeData(PlayStats.entry, NUM_PLAY_STATS_ENTRIES * sizeof(PlayStatsEntry));
			static_assert((NUM_PLAY_STATS_ENTRIES * sizeof(PlayStatsEntry)) == 0x1400);
		}

		void LoadPlaystats()
		{
			PlayStats.numEntries = 0;
			for (size_t i = 0; i < NUM_PLAY_STATS_ENTRIES; i++)
			{
				auto& e = PlayStats.entry[i];
				memset(&e, 0, sizeof(PlayStatsEntry));
			}
			PlayStats.fs = FileStream::openFile2(GetPDFile("PlayStats.dat"), true);
			if (!PlayStats.fs)
			{
				CreatePlaystats();
				return;
			}
			if (PlayStats.fs->GetSize() != (NUM_PLAY_STATS_ENTRIES * 20 + 4))
			{
				delete PlayStats.fs;
				PlayStats.fs = nullptr;
				cemuLog_log(LogType::Force, "PlayStats.dat malformed");
				// dont delete the existing file in case it could still be salvaged (todo) and instead just dont track play time
				return;
			}
			PlayStats.fs->readData(&PlayStats.numEntries, sizeof(uint32be));
			if (PlayStats.numEntries > NUM_PLAY_STATS_ENTRIES)
				PlayStats.numEntries = NUM_PLAY_STATS_ENTRIES;
			PlayStats.fs->readData(PlayStats.entry, NUM_PLAY_STATS_ENTRIES * 20);
		}

		PlayStatsEntry* PlayStats_GetEntry(uint64 titleId)
		{
			uint32be titleIdHigh = (uint32)(titleId>>32);
			uint32be titleIdLow = (uint32)(titleId & 0xFFFFFFFF);
			size_t numEntries = PlayStats.numEntries;
			for (size_t i = 0; i < numEntries; i++)
			{
				if (PlayStats.entry[i].titleIdHigh == titleIdHigh && PlayStats.entry[i].titleIdLow == titleIdLow)
					return &PlayStats.entry[i];
			}
			return nullptr;
		}

		void PlayStats_WriteEntry(PlayStatsEntry* entry, bool writeEntryCount = false)
		{
			if (!PlayStats.fs)
				return;
			size_t entryIndex = entry - PlayStats.entry;
			cemu_assert(entryIndex < NUM_PLAY_STATS_ENTRIES);
			PlayStats.fs->SetPosition(4 + entryIndex * sizeof(PlayStatsEntry));
			if (PlayStats.fs->writeData(entry, sizeof(PlayStatsEntry)) != sizeof(PlayStatsEntry))
			{
				cemuLog_log(LogType::Force, "Failed to write to PlayStats.dat");
				return;
			}
			if (writeEntryCount)
			{
				uint32be numEntries = PlayStats.numEntries;
				PlayStats.fs->SetPosition(0);
				PlayStats.fs->writeData(&numEntries, sizeof(uint32be));
			}
		}

		PlayStatsEntry* PlayStats_CreateEntry(uint64 titleId)
		{
			bool entryCountChanged = false;
			PlayStatsEntry* newEntry;
			if(PlayStats.numEntries < NUM_PLAY_STATS_ENTRIES)
			{
				newEntry = PlayStats.entry + PlayStats.numEntries;
				PlayStats.numEntries += 1;
				entryCountChanged = true;
			}
			else
			{
				// list is full - find existing entry with least amount of minutes and replace it
				newEntry = PlayStats.entry + 0;
				for (uint32 i = 1; i < NUM_PLAY_STATS_ENTRIES; i++)
				{
					if(PlayStats.entry[i].totalMinutesPlayed < newEntry->totalMinutesPlayed)
						newEntry = PlayStats.entry + i;
				}
			}
			newEntry->titleIdHigh = (uint32)(titleId >> 32);
			newEntry->titleIdLow = (uint32)(titleId & 0xFFFFFFFF);
			newEntry->firstLaunchDayIndex = GetTodaysDayIndex();
			newEntry->mostRecentDayIndex = newEntry->firstLaunchDayIndex;
			newEntry->numTimesLaunched = 1;
			newEntry->totalMinutesPlayed = 0;
			newEntry->ukn12 = 0;
			PlayStats_WriteEntry(newEntry, entryCountChanged);
			return newEntry;
		}

		// sets last played if entry already exists
		// if it does not exist it creates a new entry with first and last played set to today
		PlayStatsEntry* PlayStats_BeginNewTracking(uint64 titleId)
		{
			PlayStatsEntry* entry = PlayStats_GetEntry(titleId);
			if (entry)
			{
				entry->mostRecentDayIndex = GetTodaysDayIndex();
				entry->numTimesLaunched += 1;
				PlayStats_WriteEntry(entry);
				return entry;
			}
			return PlayStats_CreateEntry(titleId);
		}

		void PlayStats_CountAdditionalMinutes(PlayStatsEntry* entry, uint32 additionalMinutes)
		{
			if (additionalMinutes == 0)
				return;
			entry->totalMinutesPlayed += additionalMinutes;
			entry->mostRecentDayIndex = GetTodaysDayIndex();
			PlayStats_WriteEntry(entry);
		}

		struct PlayDiaryHeader
		{
			// the play diary is a rolling log
			// initially only writeIndex increases
			// after the log is filled, writeIndex wraps over and readIndex will increase as well
			uint32be readIndex;
			uint32be writeIndex;
		};

		static_assert(sizeof(PlayDiaryEntry) == 0x10);
		static_assert(sizeof(PlayDiaryHeader) == 0x8);

		struct
		{
			FileStream* fs{};
			PlayDiaryHeader header;
			PlayDiaryEntry entry[NUM_PLAY_DIARY_ENTRIES_MAX];
		}PlayDiaryData;

		void CreatePlayDiary()
		{
			MakeDirectory();
			PlayDiaryData.fs = FileStream::createFile2(GetPDFile("PlayDiary.dat"));
			if (!PlayDiaryData.fs)
			{
				cemuLog_log(LogType::Force, "Failed to read or write PlayDiary.dat, playtime tracking will not be possible");
			}
			// write header
			PlayDiaryData.header.readIndex = 0;
			PlayDiaryData.header.writeIndex = 0;
			if (PlayDiaryData.fs)
				PlayDiaryData.fs->writeData(&PlayDiaryData.header, sizeof(PlayDiaryHeader));
		}

		void LoadPlayDiary()
		{
			std::unique_lock _lock(sDiaryLock);
			cemu_assert_debug(!PlayDiaryData.fs);
			PlayDiaryData.fs = FileStream::openFile2(GetPDFile("PlayDiary.dat"), true);
			if (!PlayDiaryData.fs)
			{
				CreatePlayDiary();
				return;
			}
			// read header
			if (PlayDiaryData.fs->readData(&PlayDiaryData.header, sizeof(PlayDiaryHeader)) != sizeof(PlayDiaryHeader))
			{
				cemuLog_log(LogType::Force, "Failed to read valid PlayDiary header");
				delete PlayDiaryData.fs;
				PlayDiaryData.fs = nullptr;
				CreatePlayDiary();
				return;
			}
			if (PlayDiaryData.header.readIndex > NUM_PLAY_DIARY_ENTRIES_MAX || PlayDiaryData.header.writeIndex > NUM_PLAY_DIARY_ENTRIES_MAX)
			{
				cemuLog_log(LogType::Force, "Bad value in play diary header (read={} write={})", (uint32)PlayDiaryData.header.readIndex, (uint32)PlayDiaryData.header.writeIndex);
				PlayDiaryData.header.readIndex = PlayDiaryData.header.readIndex % NUM_PLAY_DIARY_ENTRIES_MAX;
				PlayDiaryData.header.writeIndex = PlayDiaryData.header.writeIndex % NUM_PLAY_DIARY_ENTRIES_MAX;
			}
			// read entries and set any not-yet-written entries to zero
			uint32 readBytes = PlayDiaryData.fs->readData(PlayDiaryData.entry, NUM_PLAY_DIARY_ENTRIES_MAX * sizeof(PlayDiaryEntry));
			uint32 readEntries = readBytes / sizeof(PlayDiaryEntry);
			while (readEntries < NUM_PLAY_DIARY_ENTRIES_MAX)
			{
				PlayDiaryData.entry[readEntries].titleId = 0;
				PlayDiaryData.entry[readEntries].ukn08 = 0;
				PlayDiaryData.entry[readEntries].dayIndex = 0;
				PlayDiaryData.entry[readEntries].ukn0E = 0;
				readEntries++;
			}
		}

		uint32 GetDiaryEntries(uint8 accountSlot, PlayDiaryEntry* diaryEntries, uint32 maxEntries)
		{
			std::unique_lock _lock(sDiaryLock);
			uint32 numReadEntries = 0;
			uint32 currentEntryIndex = PlayDiaryData.header.readIndex;
			while (currentEntryIndex != PlayDiaryData.header.writeIndex && numReadEntries < maxEntries)
			{
				*diaryEntries = PlayDiaryData.entry[currentEntryIndex];
				numReadEntries++;
				diaryEntries++;
				currentEntryIndex = (currentEntryIndex+1) % NUM_PLAY_DIARY_ENTRIES_MAX;
			}
			return numReadEntries;
		}

		bool GetStatForGamelist(uint64 titleId, GameListStat& stat)
		{
			stat.last_played.year = 0;
			stat.last_played.month = 0;
			stat.last_played.day = 0;
			stat.numMinutesPlayed = 0;
			std::unique_lock _lock(sDiaryLock);
			// the play stats give us last time played and the total minutes
			PlayStatsEntry* playStats = PlayStats_GetEntry(titleId);
			if (playStats)
			{
				stat.numMinutesPlayed = playStats->totalMinutesPlayed;
				chrono_d::year_month_day ymd = GetDateFromDayIndex(playStats->mostRecentDayIndex);
				stat.last_played.year = (int)ymd.year();
				stat.last_played.month = (unsigned int)ymd.month() - 1;
				stat.last_played.day = (unsigned int)ymd.day();
			}
			_lock.unlock();
			// check legacy time tracking for game entries in settings.xml
			std::unique_lock _lockGC(GetConfig().game_cache_entries_mutex);
			for (auto& gameEntry : GetConfig().game_cache_entries)
			{
				if(gameEntry.title_id != titleId)
					continue;
				stat.numMinutesPlayed += (gameEntry.legacy_time_played / 60);				
				if (gameEntry.legacy_last_played != 0)
				{
					time_t td = gameEntry.legacy_last_played;
					tm* date = localtime(&td);
					uint32 legacyYear = (uint32)date->tm_year + 1900;
					uint32 legacyMonth = (uint32)date->tm_mon;
					uint32 legacyDay = (uint32)date->tm_mday;
					if (stat.last_played.year == 0 || std::tie(legacyYear, legacyMonth, legacyDay) > std::tie(stat.last_played.year, stat.last_played.month, stat.last_played.day))
					{
						stat.last_played.year = legacyYear;
						stat.last_played.month = legacyMonth;
						stat.last_played.day = legacyDay;
					}
				}
			}
			return true;
		}

		std::thread sPDMTimeTrackingThread;
		CounterSemaphore sPDMSem;
		std::atomic_bool sPDMRequestExitThread{ false };

		void TimeTrackingThread(uint64 titleId)
		{
			PlayStatsEntry* playStatsEntry = PlayStats_BeginNewTracking(titleId);

			auto startTime = std::chrono::steady_clock::now();
			uint32 prevMinuteCounter = 0;
			while (true)
			{
				sPDMSem.decrementWithWaitAndTimeout(15000);
				if (sPDMRequestExitThread.load(std::memory_order::relaxed))
					break;
				auto currentTime = std::chrono::steady_clock::now();
				uint32 elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(currentTime - startTime).count();
				if (elapsedMinutes > prevMinuteCounter)
				{
					PlayStats_CountAdditionalMinutes(playStatsEntry, (elapsedMinutes - prevMinuteCounter));
					// todo - also update PlayDiary (and other files)
					prevMinuteCounter = elapsedMinutes;
				}
			}
		}

		void Initialize()
		{
			// todo - add support for per-account handling
			LoadPlaystats();
			LoadPlayDiary();
		}
		
		void StartTrackingTime(uint64 titleId)
		{
			sPDMRequestExitThread = false;
			sPDMTimeTrackingThread = std::thread(TimeTrackingThread, titleId);
		}

		void Stop()
		{
			sPDMRequestExitThread.store(true);
			sPDMSem.increment();
            if(sPDMTimeTrackingThread.joinable())
    			sPDMTimeTrackingThread.join();
		}

	};
};
