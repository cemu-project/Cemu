#include "nn_spm.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"

namespace nn
{
	namespace spm
	{

		struct StorageIndex
		{
			void SetInvalid() { idHigh = 0; idLow = 0; }
			void Set(uint64 id) { idHigh = id >> 32; idLow = id & 0xFFFFFFFF; }

			uint64 Get() const { return ((uint64)idHigh << 32) | (uint64)idLow; }

			uint32be idHigh;
			uint32be idLow;
		};

		enum class CemuStorageIndex
		{
			MLC = 1,
			SLC = 2,
			USB = 3,
		};

		static_assert(sizeof(StorageIndex) == 8);

		struct VolumeId
		{
			char id[16];
		};

		static_assert(sizeof(VolumeId) == 16);

		enum class StorageType : uint32
		{
			RAW,
			WFS,
		};

		struct StorageInfo
		{
			char mountPath[640]; // For example: /vol/storage_usb01
			char connectionType[8]; // usb
			char formatStr[8]; // raw / wfs
			uint8 ukn[4];
			betype<StorageType> type;
			VolumeId volumeId;
		};

		static_assert(sizeof(StorageInfo) == 680);

		struct StorageListItem
		{
			StorageIndex index;
			uint32be ukn04;
			betype<StorageType> type;
		};

		static_assert(sizeof(StorageListItem) == 16);

		sint32 GetDefaultExtendedStorageVolumeId(StorageIndex* storageIndex)
		{
			cemuLog_logDebug(LogType::Force, "GetDefaultExtendedStorageVolumeId() - stub");
			storageIndex->SetInvalid(); // we dont emulate USB storage yet
			return 0;
		}

		sint32 GetExtendedStorageIndex(StorageIndex* storageIndex)
		{
			cemuLog_logDebug(LogType::Force, "GetExtendedStorageIndex() - stub");
			storageIndex->SetInvalid(); // we dont emulate USB storage yet
			return -1; // this fails if there is none?
		}

		// nn::spm::GetStorageList((nn::spm::StorageListItem *, unsigned int))

		uint32 GetStorageList(StorageListItem* storageList, uint32 maxItems)
		{
			cemu_assert(maxItems >= 2);
			uint32 numItems = 0;

			// This should only return USB storages?
			// If we return two entries (for SLC and MLC supposedly) then the Wii U menu will complain about two usb storages

//			// mlc
//			storageList[numItems].index.Set((uint32)CemuStorageIndex::MLC);
//			storageList[numItems].ukn04 = 0;
//			storageList[numItems].type = StorageType::WFS;
//			numItems++;
//			// slc
//			storageList[numItems].index.Set((uint32)CemuStorageIndex::SLC);
//			storageList[numItems].ukn04 = 0;
//			storageList[numItems].type = StorageType::WFS;
			numItems++;
			return numItems;
		}

		sint32 GetStorageInfo(StorageInfo* storageInfo, StorageIndex* storageIndex)
		{
			cemuLog_logDebug(LogType::Force, "GetStorageInfo() - stub");
			if(storageIndex->Get() == (uint64)CemuStorageIndex::MLC)
			{
				cemu_assert_unimplemented();
			}
			else if(storageIndex->Get() == (uint64)CemuStorageIndex::SLC)
			{
				cemu_assert_unimplemented();
			}
			else
			{
				cemu_assert_unimplemented();
			}

			return 0;
		}

		sint32 VolumeId_Compare(VolumeId* volumeIdThis, VolumeId* volumeIdOther)
		{
			auto r = strncmp(volumeIdThis->id, volumeIdOther->id, 16);
			cemuLog_logDebug(LogType::Force, "VolumeId_Compare(\"{}\", \"{}\")", volumeIdThis->id, volumeIdOther->id);
			return (sint32)r;
		}

		sint32 WaitStateUpdated(uint64be* waitState)
		{
			// WaitStateUpdated__Q2_2nn3spmFPUL
			cemuLog_logDebug(LogType::Force, "WaitStateUpdated() called");
			*waitState = 1;
			return 0;
		}

		void load()
		{
			cafeExportRegisterFunc(GetDefaultExtendedStorageVolumeId, "nn_spm", "GetDefaultExtendedStorageVolumeId__Q2_2nn3spmFv", LogType::Placeholder);
			cafeExportRegisterFunc(GetExtendedStorageIndex, "nn_spm", "GetExtendedStorageIndex__Q2_2nn3spmFPQ3_2nn3spm12StorageIndex", LogType::Placeholder);
			cafeExportRegisterFunc(GetStorageList, "nn_spm", "GetStorageList__Q2_2nn3spmFPQ3_2nn3spm15StorageListItemUi", LogType::Placeholder);

			cafeExportRegisterFunc(GetStorageInfo, "nn_spm", "GetStorageInfo__Q2_2nn3spmFPQ3_2nn3spm11StorageInfoQ3_2nn3spm12StorageIndex", LogType::Placeholder);



			cafeExportRegisterFunc(VolumeId_Compare, "nn_spm", "Compare__Q3_2nn3spm8VolumeIdCFRCQ3_2nn3spm8VolumeId", LogType::Placeholder);

			cafeExportRegisterFunc(WaitStateUpdated, "nn_spm", "WaitStateUpdated__Q2_2nn3spmFPUL", LogType::Placeholder);
		}
	}
}
