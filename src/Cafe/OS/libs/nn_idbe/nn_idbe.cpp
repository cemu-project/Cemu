#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_acp/nn_acp.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/nn_common.h"
#include "util/crypto/aes128.h"
#include "openssl/sha.h"
#include "Cemu/napi/napi.h"

namespace nn
{
	namespace idbe
	{
		struct nnIdbeIconDataV0_t
		{
			// raw icon data as byte array
			// check NAPI::IDBEIconDataV0 for exact data layout
			uint8 rawData[0x12060];
			uint8* GetTGAData()
			{
				return rawData + 0x2030;
			}
		};

		static_assert(sizeof(nnIdbeIconDataV0_t) == 0x12060, "");

		struct nnIdbeHeader_t
		{
			uint8 formatVersion;
			uint8 keyIndex;
		};

		struct nnIdbeEncryptedIcon_t
		{
			nnIdbeHeader_t header;
			uint8 hashSHA256[32];
			nnIdbeIconDataV0_t iconData;
		};

		static_assert(offsetof(nnIdbeEncryptedIcon_t, hashSHA256) == 2, "");
		static_assert(offsetof(nnIdbeEncryptedIcon_t, iconData) == 0x22, "");
		static_assert(sizeof(nnIdbeEncryptedIcon_t) == 0x12082);

		void asyncDownloadIconFile(uint64 titleId, nnIdbeEncryptedIcon_t* iconOut, coreinit::OSEvent* event)
		{
			std::vector<uint8> idbeData = NAPI::IDBE_RequestRawEncrypted(ActiveSettings::GetNetworkService(), titleId);
			if (idbeData.size() != sizeof(nnIdbeEncryptedIcon_t))
			{
				// icon does not exist or has the wrong size
				cemuLog_log(LogType::Force, "IDBE: Failed to retrieve icon for title {:016x}", titleId);
				memset(iconOut, 0, sizeof(nnIdbeEncryptedIcon_t));
				coreinit::OSSignalEvent(event);
				return;
			}
			memcpy(iconOut, idbeData.data(), sizeof(nnIdbeEncryptedIcon_t));
			coreinit::OSSignalEvent(event);
		}

		void export_DownloadIconFile(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(encryptedIconData, nnIdbeEncryptedIcon_t, 0);
			ppcDefineParamU64(titleId, 2);
			ppcDefineParamU32(uknR7, 4);
			ppcDefineParamU32(uknR8, 5);

			StackAllocator<coreinit::OSEvent> event;
			coreinit::OSInitEvent(&event, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_AUTO);
			auto asyncTask = std::async(std::launch::async, asyncDownloadIconFile, titleId, encryptedIconData, &event);
			coreinit::OSWaitEvent(&event);
			osLib_returnFromFunction(hCPU, 1);
		}

		static_assert(sizeof(nnIdbeHeader_t) == 0x2, "");

		static uint8 idbeAesKeys[4 * 16] =
		{
			0x4A,0xB9,0xA4,0x0E,0x14,0x69,0x75,0xA8,0x4B,0xB1,0xB4,0xF3,0xEC,0xEF,0xC4,0x7B,
			0x90,0xA0,0xBB,0x1E,0x0E,0x86,0x4A,0xE8,0x7D,0x13,0xA6,0xA0,0x3D,0x28,0xC9,0xB8,
			0xFF,0xBB,0x57,0xC1,0x4E,0x98,0xEC,0x69,0x75,0xB3,0x84,0xFC,0xF4,0x07,0x86,0xB5,
			0x80,0x92,0x37,0x99,0xB4,0x1F,0x36,0xA6,0xA7,0x5F,0xB8,0xB4,0x8C,0x95,0xF6,0x6F
		};

		static uint8 idbeAesIv[16] =
		{
			0xA4,0x69,0x87,0xAE,0x47,0xD8,0x2B,0xB4,0xFA,0x8A,0xBC,0x04,0x50,0x28,0x5F,0xA4
		};

		bool decryptIcon(nnIdbeEncryptedIcon_t* iconInput, nnIdbeIconDataV0_t* iconOutput)
		{
			// check header
			nnIdbeHeader_t* idbeHeader = (nnIdbeHeader_t*)iconInput;
			if (idbeHeader->formatVersion != 0)
			{
				cemuLog_log(LogType::Force, "idbe header version unknown ({})", (sint32)idbeHeader->formatVersion);
				return false;
			}
			if (idbeHeader->keyIndex >= 4)
			{
				cemuLog_log(LogType::Force, "idbe header key count invalid ({})", (sint32)idbeHeader->keyIndex);
				return false;
			}
			// decrypt data
			uint8 iv[16];
			memcpy(iv, idbeAesIv, sizeof(iv));
			uint8 decryptedSHA256[SHA256_DIGEST_LENGTH];
			AES128_CBC_decrypt_updateIV(decryptedSHA256, iconInput->hashSHA256, sizeof(decryptedSHA256), idbeAesKeys + 16 * idbeHeader->keyIndex, iv);
			AES128_CBC_decrypt((uint8*)iconOutput, (uint8*)&iconInput->iconData, sizeof(iconInput->iconData), idbeAesKeys + 16 * idbeHeader->keyIndex, iv);
			// calculate and compare sha256
			uint8 calculatedSHA256[SHA256_DIGEST_LENGTH];
			SHA256((const unsigned char*)iconOutput, sizeof(nnIdbeIconDataV0_t), calculatedSHA256);
			if (memcmp(calculatedSHA256, decryptedSHA256, SHA256_DIGEST_LENGTH) != 0)
			{
				cemuLog_logDebug(LogType::Force, "Idbe icon has incorrect sha256 hash");
				return false;
			}
			return true;
		}

		struct TGAHeader
		{
			/* +0x00 */ uint8 idLength;
			/* +0x01 */ uint8 colorMap;
			/* +0x02 */ uint8 imageType;
			/* +0x03 */	uint8 colorMap_firstIndex_low;
			/* +0x04 */ uint8 colorMap_firstIndex_high;
			/* +0x05 */ uint8 colorMap_len_low;
			/* +0x06 */ uint8 colorMap_len_high;
			/* +0x07 */ uint8 colorMap_bpp;
			/* +0x08 */ uint16 image_xOrigin;
			/* +0x0A */ uint16 image_yOrigin;
			/* +0x0C */ uint16 image_width;
			/* +0x0E */ uint16 image_height;
			/* +0x10 */ uint8 image_bpp;
			/* +0x11 */ uint8 image_desc;
		};

		static_assert(offsetof(TGAHeader, colorMap_firstIndex_low) == 0x03);
		static_assert(sizeof(TGAHeader) == 0x12);

		void export_DecryptIconFile(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(output, nnIdbeIconDataV0_t, 0);
			ppcDefineParamTypePtr(input, nnIdbeEncryptedIcon_t, 1);
			ppcDefineParamU32(platformMode, 2);

			cemuLog_logDebug(LogType::Force, "nn_idbe.DecryptIconFile(...)");

			if (decryptIcon(input, output))
			{
				osLib_returnFromFunction(hCPU, 1);
				return;
			}
			cemuLog_logDebug(LogType::Force, "Unable to decrypt idbe icon file, using default icon");

			// return default icon
			TGAHeader* tgaHeader = (TGAHeader*)(output->GetTGAData());
			memset(tgaHeader, 0, sizeof(TGAHeader));
			tgaHeader->imageType = 2;

			tgaHeader->image_width = 256;
			tgaHeader->image_height = 256;
			tgaHeader->image_bpp = 32;
			tgaHeader->image_desc = (1 << 3);

			osLib_returnFromFunction(hCPU, 1);
		}

		void load()
		{
			// this module is used by:
			// Daily Log app
			// Download Manager app
			// and possibly other system titles?

			osLib_addFunction("nn_idbe", "DownloadIconFile__Q2_2nn4idbeFPvULUsb", export_DownloadIconFile);
			osLib_addFunction("nn_idbe", "DecryptIconFile__Q2_2nn4idbeFPvPCv", export_DecryptIconFile);
		}

	}
}
