#include "Cafe/OS/common/OSCommon.h"
#include "config/ActiveSettings.h"
#include "Cafe/TitleList/TitleId.h"

#include "Cafe/IOSU/PDM/iosu_pdm.h"

#include "nn_pdm.h"

namespace nn
{
	namespace pdm
	{
		using PlayDiary = iosu::pdm::PlayDiaryEntry;

		uint32 GetPlayDiaryMaxLength(uint32be* count)
		{
			*count = iosu::pdm::NUM_PLAY_DIARY_ENTRIES_MAX;
			return 0;
		}

		uint32 GetPlayStatsMaxLength(uint32be* count)
		{
			*count = iosu::pdm::NUM_PLAY_STATS_ENTRIES;
			return 0;
		}

		uint32 GetPlayDiary(uint32be* ukn1, PlayDiary* playDiary, uint32 accountSlot, uint32 maxNumEntries)
		{
			uint32 numReadEntries = iosu::pdm::GetDiaryEntries(accountSlot, playDiary, maxNumEntries);
			*ukn1 = numReadEntries;
			return 0;
		}

		void Initialize()
		{
			cafeExportRegisterFunc(GetPlayDiaryMaxLength, "nn_pdm", "GetPlayDiaryMaxLength__Q2_2nn3pdmFPi", LogType::NN_PDM);
			cafeExportRegisterFunc(GetPlayStatsMaxLength, "nn_pdm", "GetPlayStatsMaxLength__Q2_2nn3pdmFPi", LogType::NN_PDM);

			cafeExportRegisterFunc(GetPlayDiary, "nn_pdm", "GetPlayDiary__Q2_2nn3pdmFPiPQ3_2nn3pdm9PlayDiaryiT3", LogType::NN_PDM);
		}
	}
}
