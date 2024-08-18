#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/snd_user/snd_user.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/RPL/rpl.h"

using namespace snd_core;

namespace snd
{
	namespace user
	{
#define AX_MAX_NUM_DRC (2)
#define AX_MAX_NUM_RMT (4)

#define AX_UPDATE_MODE_10000000 (0x10000000)
#define AX_UPDATE_MODE_20000000 (0x20000000)
#define AX_UPDATE_MODE_40000000_VOLUME (0x40000000)
#define AX_UPDATE_MODE_50000000 (0x50000000)
#define AX_UPDATE_MODE_40000007 (0x40000007)
#define AX_UPDATE_MODE_80000000 (0x80000000)

		struct VolumeData
		{
			sint16 volume; // 0x00
			sint16 volume_target; // 0x02
		};
		static_assert(sizeof(VolumeData) == 0x4, "sizeof(VolumeData)");

		struct MixControl
		{
			sint16 aux[AX_AUX_BUS_COUNT];
			sint16 pan;
			sint16 span;
			sint16 fader;
			sint16 lfe;
		};
		static_assert(sizeof(MixControl) == 0xE, "sizeof(MixControl)");

		using MixMode = uint32_t;

		struct MixChannel
		{
			MEMPTR<AXVPB> voice;

			uint32 update_mode;
			sint16 input_level;
			VolumeData volume;

			MixControl tv_control;
			sint16 tv_channels[AX_TV_CHANNEL_COUNT];
			VolumeData tv_volume[AX_MAX_NUM_BUS][AX_TV_CHANNEL_COUNT];
			MixMode tv_mode;

			MixControl drc_control[AX_MAX_NUM_DRC];
			sint16 drc_channels[AX_MAX_NUM_DRC][AX_DRC_CHANNEL_COUNT];
			VolumeData drc_volume[AX_MAX_NUM_DRC][AX_MAX_NUM_BUS][AX_DRC_CHANNEL_COUNT];
			MixMode drc_mode[AX_MAX_NUM_DRC];

			MixControl rmt_control[AX_MAX_NUM_RMT];
			sint16 rmt_channels[AX_MAX_NUM_RMT][AX_RMT_CHANNEL_COUNT];
			VolumeData rmt_volume[AX_MAX_NUM_RMT][AX_MAX_NUM_BUS][AX_RMT_CHANNEL_COUNT];
			MixMode rmt_mode[AX_MAX_NUM_RMT];

			MixControl& GetMixControl(uint32 device, uint32 deviceIndex)
			{
				if (device == AX_DEV_TV)
				{
					cemu_assert(deviceIndex == 0);
					return tv_control;
				}
				else if (device == AX_DEV_DRC)
				{
					cemu_assert(deviceIndex < AX_MAX_NUM_DRC);
					return drc_control[deviceIndex];
				}
				else if (device == AX_DEV_RMT)
				{
					cemu_assert(deviceIndex < AX_MAX_NUM_RMT);
					return rmt_control[deviceIndex];
				}
				cemuLog_log(LogType::Force, "GetMixControl({}, {}): Invalid device/deviceIndex", device, deviceIndex);
				cemu_assert(false);
				return tv_control;
			}

			MixMode& GetMode(uint32 device, uint32 deviceIndex)
			{
				if (device == AX_DEV_TV)
				{
					cemu_assert(deviceIndex == 0);
					return tv_mode;
				}
				else if (device == AX_DEV_DRC)
				{
					cemu_assert(deviceIndex < AX_MAX_NUM_DRC);
					return drc_mode[deviceIndex];
				}
				else if (device == AX_DEV_RMT)
				{
					cemu_assert(deviceIndex < AX_MAX_NUM_RMT);
					return rmt_mode[deviceIndex];
				}
				cemuLog_log(LogType::Force, "GetMode({}, {}): Invalid device/deviceIndex", device, deviceIndex);
				cemu_assert(false);
				return tv_mode;
			}

			sint16* GetChannels(uint32 device, uint32 deviceIndex)
			{
				if (device == AX_DEV_TV)
				{
					cemu_assert(deviceIndex == 0);
					return tv_channels;
				}
				else if (device == AX_DEV_DRC)
				{
					cemu_assert(deviceIndex < AX_MAX_NUM_DRC);
					return drc_channels[deviceIndex];
				}
				else if (device == AX_DEV_RMT)
				{
					cemu_assert(deviceIndex < AX_MAX_NUM_RMT);
					return rmt_channels[deviceIndex];
				}
				cemuLog_log(LogType::Force, "GetChannels({}, {}): Invalid device/deviceIndex", device, deviceIndex);
				cemu_assert(false);
				return tv_channels;
			}
		};
		static_assert(sizeof(MixChannel) == 0x1D0, "sizeof(MixChannel)");

		struct DeviceInfo
		{
			uint32 tv_sound_mode; // 0x00
			uint32 drc_sound_mode; // 0x04
			uint32 rmt_sound_mode; // 0x08
		};

		struct snd_user_data_t
		{
			bool initialized;

			DeviceInfo device_info;
			sint32 max_voices;

			MixChannel mix_channel[AX_MAX_VOICES];

			const uint16 volume[0x388 + 0x3C + 1] =
			{
				0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
				2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
				5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xC,
				0xC, 0xC, 0xC, 0xC, 0xC, 0xC, 0xD, 0xD, 0xD, 0xD, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xE, 0xE, 0xE, 0xF, 0xF, 0xF, 0xF, 0xF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13,
				0x13, 0x13, 0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x1A, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C, 0x1D, 0x1D, 0x1D, 0x1E,
				0x1E, 0x1E, 0x1F, 0x1F, 0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E,
				0x2F, 0x2F, 0x30, 0x31, 0x31, 0x32, 0x32, 0x33, 0x33, 0x34, 0x35, 0x35, 0x36, 0x37, 0x37, 0x38, 0x38, 0x39, 0x3A, 0x3A, 0x3B, 0x3C, 0x3D, 0x3D, 0x3E, 0x3F, 0x3F, 0x40, 0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x46, 0x46, 0x47, 0x48,
				 0x49, 0x4A, 0x4B, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x64, 0x65, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x6D, 0x6F, 0x70,
				0x71, 0x72, 0x74, 0x75, 0x76, 0x78, 0x79, 0x7B, 0x7C, 0x7E, 0x7F, 0x80, 0x82, 0x83, 0x85, 0x87, 0x88, 0x8A, 0x8B, 0x8D, 0x8F, 0x90, 0x92, 0x94, 0x95, 0x97, 0x99, 0x9B, 0x9C, 0x9E, 0xA0, 0xA2, 0xA4, 0xA6, 0xA8, 0xAA, 0xAB, 0xAD,
				0xAF, 0xB2, 0xB4, 0xB6, 0xB8, 0xBA, 0xBC, 0xBE, 0xC0, 0xC3, 0xC5, 0xC7, 0xCA, 0xCC, 0xCE, 0xD1, 0xD3, 0xD6, 0xD8, 0xDB, 0xDD, 0xE0, 0xE2, 0xE5, 0xE7, 0xEA, 0xED, 0xF0, 0xF2, 0xF5, 0xF8, 0xFB, 0xFE, 0x101, 0x104, 0x107, 0x10A, 0x10D,
				0x110, 0x113, 0x116, 0x11A, 0x11D, 0x120, 0x124, 0x127, 0x12A, 0x12E, 0x131, 0x135, 0x138, 0x13C, 0x140, 0x143, 0x147, 0x14B, 0x14F, 0x153, 0x157, 0x15B, 0x15F, 0x163, 0x167, 0x16B, 0x16F, 0x173, 0x178, 0x17C, 0x180, 0x185, 0x189,
				0x18E, 0x193, 0x197, 0x19C, 0x1A1, 0x1A6, 0x1AB, 0x1AF, 0x1B4, 0x1BA, 0x1BF, 0x1C4, 0x1C9, 0x1CE, 0x1D4, 0x1D9, 0x1DF, 0x1E4, 0x1EA, 0x1EF, 0x1F5, 0x1FB, 0x201, 0x207, 0x20D, 0x213, 0x219, 0x21F, 0x226, 0x22C, 0x232, 0x239, 0x240,
				0x246, 0x24D, 0x254, 0x25B, 0x262, 0x269, 0x270, 0x277, 0x27E, 0x286, 0x28D, 0x295, 0x29D, 0x2A4, 0x2AC, 0x2B4, 0x2BC, 0x2C4, 0x2CC, 0x2D5, 0x2DD, 0x2E6, 0x2EE, 0x2F7, 0x300, 0x309, 0x312, 0x31B, 0x324, 0x32D, 0x337, 0x340, 0x34A,
				0x354, 0x35D, 0x367, 0x371, 0x37C, 0x386, 0x390, 0x39B, 0x3A6, 0x3B1, 0x3BB, 0x3C7, 0x3D2, 0x3DD, 0x3E9, 0x3F4, 0x400, 0x40C, 0x418, 0x424, 0x430, 0x43D, 0x449, 0x456, 0x463, 0x470, 0x47D, 0x48A, 0x498, 0x4A5, 0x4B3, 0x4C1, 0x4CF,
				0x4DD, 0x4EC, 0x4FA, 0x509, 0x518, 0x527, 0x536, 0x546, 0x555, 0x565, 0x575, 0x586, 0x596, 0x5A6, 0x5B7, 0x5C8, 0x5D9, 0x5EB, 0x5FC, 0x60E, 0x620, 0x632, 0x644, 0x657, 0x66A, 0x67D, 0x690, 0x6A4, 0x6B7, 0x6CB, 0x6DF, 0x6F4, 0x708,
				0x71D, 0x732, 0x748, 0x75D, 0x773, 0x789, 0x79F, 0x7B6, 0x7CD, 0x7E4, 0x7FB, 0x813, 0x82B, 0x843, 0x85C, 0x874, 0x88E, 0x8A7, 0x8C1, 0x8DA, 0x8F5, 0x90F, 0x92A, 0x945, 0x961, 0x97D, 0x999, 0x9B5, 0x9D2, 0x9EF, 0xA0D, 0xA2A, 0xA48,
				0xA67, 0xA86, 0xAA5, 0xAC5, 0xAE5, 0xB05, 0xB25, 0xB47, 0xB68, 0xB8A, 0xBAC, 0xBCF, 0xBF2, 0xC15, 0xC39, 0xC5D, 0xC82, 0xCA7, 0xCCC, 0xCF2, 0xD19, 0xD3F, 0xD67, 0xD8E, 0xDB7, 0xDDF, 0xE08, 0xE32, 0xE5C, 0xE87, 0xEB2, 0xEDD, 0xF09,
				0xF36, 0xF63, 0xF91, 0xFBF, 0xFEE, 0x101D, 0x104D, 0x107D, 0x10AE, 0x10DF, 0x1111, 0x1144, 0x1177, 0x11AB, 0x11DF, 0x1214, 0x124A, 0x1280, 0x12B7, 0x12EE, 0x1326, 0x135F, 0x1399, 0x13D3, 0x140D, 0x1449, 0x1485, 0x14C2, 0x14FF, 0x153E,
				0x157D, 0x15BC, 0x15FD, 0x163E, 0x1680, 0x16C3, 0x1706, 0x174A, 0x178F, 0x17D5, 0x181C, 0x1863, 0x18AC, 0x18F5, 0x193F, 0x198A, 0x19D5, 0x1A22, 0x1A6F, 0x1ABE, 0x1B0D, 0x1B5D, 0x1BAE, 0x1C00, 0x1C53, 0x1CA7, 0x1CFC, 0x1D52, 0x1DA9,
				0x1E01, 0x1E5A, 0x1EB4, 0x1F0F, 0x1F6B, 0x1FC8, 0x2026, 0x2086, 0x20E6, 0x2148, 0x21AA, 0x220E, 0x2273, 0x22D9, 0x2341, 0x23A9, 0x2413, 0x247E, 0x24EA, 0x2557, 0x25C6, 0x2636, 0x26A7, 0x271A, 0x278E, 0x2803, 0x287A, 0x28F2, 0x296B,
				0x29E6, 0x2A62, 0x2AE0, 0x2B5F, 0x2BDF, 0x2C61, 0x2CE5, 0x2D6A, 0x2DF1, 0x2E79, 0x2F03, 0x2F8E, 0x301B, 0x30AA, 0x313A, 0x31CC, 0x325F, 0x32F5, 0x338C, 0x3425, 0x34BF, 0x355B, 0x35FA, 0x369A, 0x373C, 0x37DF, 0x3885, 0x392C, 0x39D6,
				0x3A81, 0x3B2F, 0x3BDE, 0x3C90, 0x3D43, 0x3DF9, 0x3EB1, 0x3F6A, 0x4026, 0x40E5, 0x41A5, 0x4268, 0x432C, 0x43F4, 0x44BD, 0x4589, 0x4657, 0x4727, 0x47FA, 0x48D0, 0x49A8, 0x4A82, 0x4B5F, 0x4C3E, 0x4D20, 0x4E05, 0x4EEC, 0x4FD6, 0x50C3,
				0x51B2, 0x52A4, 0x5399, 0x5491, 0x558C, 0x5689, 0x578A, 0x588D, 0x5994, 0x5A9D, 0x5BAA, 0x5CBA, 0x5DCD, 0x5EE3, 0x5FFC, 0x6119, 0x6238, 0x635C, 0x6482, 0x65AC, 0x66D9, 0x680A, 0x693F, 0x6A77, 0x6BB2, 0x6CF2, 0x6E35, 0x6F7B, 0x70C6,
				0x7214, 0x7366, 0x74BC, 0x7616, 0x7774, 0x78D6, 0x7A3D, 0x7BA7, 0x7D16, 0x7E88, 0x7FFF, 0x817B, 0x82FB, 0x847F, 0x8608, 0x8795, 0x8927, 0x8ABE, 0x8C59, 0x8DF9, 0x8F9E, 0x9148, 0x92F6, 0x94AA, 0x9663, 0x9820, 0x99E3, 0x9BAB, 0x9D79,
				0x9F4C, 0xA124, 0xA302, 0xA4E5, 0xA6CE, 0xA8BC, 0xAAB0, 0xACAA, 0xAEAA, 0xB0B0, 0xB2BC, 0xB4CE, 0xB6E5, 0xB904, 0xBB28, 0xBD53, 0xBF84, 0xC1BC, 0xC3FA, 0xC63F, 0xC88B, 0xCADD, 0xCD37, 0xCF97, 0xD1FE, 0xD46D, 0xD6E3, 0xD960, 0xDBE4,
				0xDE70, 0xE103, 0xE39E, 0xE641, 0xE8EB, 0xEB9E, 0xEE58, 0xF11B, 0xF3E6, 0xF6B9, 0xF994, 0xFC78, 0xFF64
			};

			const uint32 pan_values[128] =
			{
				00, 00,
				0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFE, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFD, 0xFFFFFFFC, 0xFFFFFFFC, 0xFFFFFFFC, 0xFFFFFFFB, 0xFFFFFFFB, 0xFFFFFFFB,
				0xFFFFFFFA, 0xFFFFFFFA, 0xFFFFFFF9, 0xFFFFFFF9, 0xFFFFFFF9, 0xFFFFFFF8, 0xFFFFFFF8, 0xFFFFFFF7, 0xFFFFFFF7, 0xFFFFFFF6, 0xFFFFFFF6, 0xFFFFFFF6, 0xFFFFFFF5, 0xFFFFFFF5,
				0xFFFFFFF4, 0xFFFFFFF4, 0xFFFFFFF3, 0xFFFFFFF3, 0xFFFFFFF2, 0xFFFFFFF2, 0xFFFFFFF2, 0xFFFFFFF1, 0xFFFFFFF1, 0xFFFFFFF0, 0xFFFFFFF0, 0xFFFFFFEF, 0xFFFFFFEF, 0xFFFFFFEE,
				0xFFFFFFEE,	0xFFFFFFED, 0xFFFFFFEC, 0xFFFFFFEC, 0xFFFFFFEB, 0xFFFFFFEB, 0xFFFFFFEA, 0xFFFFFFEA, 0xFFFFFFE9, 0xFFFFFFE9, 0xFFFFFFE8, 0xFFFFFFE7, 0xFFFFFFE7, 0xFFFFFFE6,
				0xFFFFFFE6,	0xFFFFFFE5, 0xFFFFFFE4, 0xFFFFFFE4, 0xFFFFFFE3, 0xFFFFFFE2, 0xFFFFFFE2, 0xFFFFFFE1, 0xFFFFFFE0, 0xFFFFFFDF, 0xFFFFFFDF, 0xFFFFFFDE, 0xFFFFFFDD, 0xFFFFFFDC,
				0xFFFFFFDC, 0xFFFFFFDB,	0xFFFFFFDA, 0xFFFFFFD9, 0xFFFFFFD8, 0xFFFFFFD8, 0xFFFFFFD7, 0xFFFFFFD6, 0xFFFFFFD5, 0xFFFFFFD4, 0xFFFFFFD3, 0xFFFFFFD2, 0xFFFFFFD1, 0xFFFFFFD0,
				0xFFFFFFCF, 0xFFFFFFCE, 0xFFFFFFCD, 0xFFFFFFCC, 0xFFFFFFCA, 0xFFFFFFC9, 0xFFFFFFC8, 0xFFFFFFC7, 0xFFFFFFC5, 0xFFFFFFC4, 0xFFFFFFC3, 0xFFFFFFC1, 0xFFFFFFC0, 0xFFFFFFBE,
				0xFFFFFFBD, 0xFFFFFFBB, 0xFFFFFFB9,	0xFFFFFFB8, 0xFFFFFFB6, 0xFFFFFFB4, 0xFFFFFFB2, 0xFFFFFFB0, 0xFFFFFFAD, 0xFFFFFFAB, 0xFFFFFFA9, 0xFFFFFFA6, 0xFFFFFFA3, 0xFFFFFFA0,
				0xFFFFFF9D, 0xFFFFFF9A, 0xFFFFFF96,	0xFFFFFF92, 0xFFFFFF8D, 0xFFFFFF88, 0xFFFFFF82, 0xFFFFFF7B, 0xFFFFFF74, 0xFFFFFF6A, 0xFFFFFF5D, 0xFFFFFF4C, 0xFFFFFF2E, 0xFFFFFC78
			};

			const uint16 pan_values_low[128] =
			{
			 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFE, 0xFFFE, 0xFFFE, 0xFFFE, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFC, 0xFFFC, 0xFFFC, 0xFFFB, 0xFFFB, 0xFFFA, 0xFFFA, 0xFFFA, 0xFFF9, 0xFFF9, 0xFFF8, 0xFFF8,
			0xFFF7, 0xFFF7, 0xFFF6, 0xFFF5, 0xFFF5, 0xFFF4, 0xFFF4, 0xFFF3, 0xFFF2, 0xFFF2, 0xFFF1, 0xFFF0, 0xFFEF, 0xFFEF, 0xFFEE, 0xFFED, 0xFFEC, 0xFFEB, 0xFFEB, 0xFFEA, 0xFFE9, 0xFFE8, 0xFFE7, 0xFFE6, 0xFFE5, 0xFFE4, 0xFFE3, 0xFFE2, 0xFFE1,
			0xFFE0, 0xFFDE, 0xFFDD, 0xFFDC, 0xFFDB, 0xFFDA, 0xFFD8, 0xFFD7, 0xFFD6, 0xFFD4, 0xFFD3, 0xFFD1, 0xFFD0, 0xFFCE, 0xFFCC, 0xFFCB, 0xFFC9, 0xFFC7, 0xFFC6, 0xFFC4, 0xFFC2, 0xFFC0, 0xFFBE, 0xFFBC, 0xFFBA, 0xFFB7, 0xFFB5, 0xFFB3, 0xFFB0,
			0xFFAE, 0xFFAB, 0xFFA8, 0xFFA6, 0xFFA3, 0xFFA0, 0xFF9C, 0xFF99, 0xFF96, 0xFF92, 0xFF8E, 0xFF8A, 0xFF86, 0xFF82, 0xFF7D, 0xFF78, 0xFF73, 0xFF6E, 0xFF68, 0xFF61, 0xFF5A, 0xFF53, 0xFF4B, 0xFF42, 0xFF37, 0xFF2C, 0xFF1F, 0xFF0F, 0xFEFB,
			0xFEE2, 0xFEBF, 0xFE83, 0xFC40
			};

			const uint16 pan_values_high[128] =
			{
			0xFFC3, 0xFFC3, 0xFFC4, 0xFFC5, 0xFFC5, 0xFFC6, 0xFFC6, 0xFFC7, 0xFFC8, 0xFFC8, 0xFFC9, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCB, 0xFFCC, 0xFFCC, 0xFFCD, 0xFFCE, 0xFFCE, 0xFFCF, 0xFFCF, 0xFFD0, 0xFFD0, 0xFFD1, 0xFFD1, 0xFFD2, 0xFFD2, 0xFFD3,
			0xFFD3, 0xFFD4, 0xFFD4, 0xFFD5, 0xFFD5, 0xFFD6, 0xFFD6, 0xFFD7, 0xFFD7, 0xFFD8, 0xFFD8, 0xFFD9, 0xFFD9, 0xFFDA, 0xFFDA, 0xFFDA, 0xFFDB, 0xFFDB, 0xFFDC, 0xFFDC, 0xFFDD, 0xFFDD, 0xFFDD, 0xFFDE, 0xFFDE, 0xFFDF, 0xFFDF, 0xFFE0, 0xFFE0,
			0xFFE0, 0xFFE1, 0xFFE1, 0xFFE1, 0xFFE2, 0xFFE2, 0xFFE3, 0xFFE3, 0xFFE3, 0xFFE4, 0xFFE4, 0xFFE4, 0xFFE5, 0xFFE5, 0xFFE5, 0xFFE6, 0xFFE6, 0xFFE6, 0xFFE7, 0xFFE7, 0xFFE7, 0xFFE8, 0xFFE8, 0xFFE8, 0xFFE9, 0xFFE9, 0xFFE9, 0xFFEA, 0xFFEA,
			0xFFEA, 0xFFEB, 0xFFEB, 0xFFEB, 0xFFEC, 0xFFEC, 0xFFEC, 0xFFEC, 0xFFED, 0xFFED, 0xFFED, 0xFFEE, 0xFFEE, 0xFFEE, 0xFFEE, 0xFFEF, 0xFFEF, 0xFFEF, 0xFFEF, 0xFFF0, 0xFFF0, 0xFFF0, 0xFFF0, 0xFFF1, 0xFFF1, 0xFFF1, 0xFFF1, 0xFFF2, 0xFFF2,
			0xFFF2, 0xFFF2, 0xFFF3, 0xFFF3, 0xFFF3, 0xFFF3, 0xFFF3, 0xFFF4, 0xFFF4, 0xFFF4, 0xFFF4, 0xFFF5
			};

		} g_snd_user_data{};

		void _MIXChannelResetTV(MixChannel* channel, sint32 index)
		{
			assert(index == 0);

			channel->tv_mode = 0;

			channel->tv_control.pan = 0x40;
			channel->tv_control.span = 0x7F;
			channel->tv_control.fader = 0;
			channel->tv_control.lfe = -960;

			for (size_t i = 0; i < AX_AUX_BUS_COUNT; ++i)
			{
				channel->tv_control.aux[i] = -960;
			}

			for (size_t i = 0; i < AX_MAX_NUM_BUS; ++i)
			{
				for (size_t j = 0; j < AX_TV_CHANNEL_COUNT; ++j)
				{
					channel->tv_volume[i][j].volume = 0;
					channel->tv_volume[i][j].volume_target = 0;
				}
			}
		}

		void _MIXChannelResetDRC(MixChannel* channel, sint32 index)
		{
			assert(index < AX_MAX_NUM_DRC);
			channel->drc_mode[index] = 0;

			channel->drc_control[index].pan = 0x40;
			channel->drc_control[index].span = 0x7F;
			channel->drc_control[index].fader = 0;
			channel->drc_control[index].lfe = -960;

			for (size_t i = 0; i < AX_AUX_BUS_COUNT; ++i)
			{
				channel->drc_control[index].aux[i] = -960;
			}

			for (size_t i = 0; i < AX_MAX_NUM_BUS; ++i)
			{
				for (size_t j = 0; j < AX_DRC_CHANNEL_COUNT; ++j)
				{
					channel->drc_volume[index][i][j].volume = 0;
					channel->drc_volume[index][i][j].volume_target = 0;
				}
			}
		}

		void _MIXChannelResetRmt(MixChannel* channel, sint32 index)
		{
			assert(index < AX_MAX_NUM_RMT);
			channel->rmt_mode[index] = 0;

			channel->rmt_control[index].pan = 0x40;
			channel->rmt_control[index].span = 0x7F;
			channel->rmt_control[index].fader = 0;
			channel->rmt_control[index].lfe = -960;

			for (size_t i = 0; i < AX_AUX_BUS_COUNT; ++i)
			{
				channel->rmt_control[index].aux[i] = -960;
			}

			for (size_t i = 0; i < AX_MAX_NUM_BUS; ++i)
			{
				for (size_t j = 0; j < AX_RMT_CHANNEL_COUNT; ++j)
				{
					channel->rmt_volume[index][i][j].volume = 0;
					channel->rmt_volume[index][i][j].volume_target = 0;
				}
			}
		}

		void MIXResetChannelData(MixChannel* channel)
		{
			channel->update_mode = AX_UPDATE_MODE_50000000;
			channel->input_level = 0;
			channel->volume.volume = 0;
			channel->volume.volume_target = 0;

			_MIXChannelResetTV(channel, 0);

			for (int i = 0; i < AX_MAX_NUM_DRC; ++i)
				_MIXChannelResetDRC(channel, i);

			for (int i = 0; i < AX_MAX_NUM_RMT; ++i)
				_MIXChannelResetRmt(channel, i);
		}

		void _MIXControl_SetDevicePan(MixControl* control, int device_type, sint16 channels[])
		{
			const auto pandiff = 0x7F - control->pan;
			const auto spandiff = 0x7F - control->span;

			if (device_type == AX_DEV_TV)
			{
				const uint32 sound_mode = g_snd_user_data.device_info.tv_sound_mode;
				if (sound_mode == 3)
				{
					channels[0] = g_snd_user_data.pan_values_low[control->pan];
					channels[1] = g_snd_user_data.pan_values_low[pandiff];
					channels[2] = g_snd_user_data.pan_values_high[pandiff];
					channels[3] = g_snd_user_data.pan_values_high[control->pan];
					channels[4] = g_snd_user_data.pan_values_high[spandiff];
					channels[5] = g_snd_user_data.pan_values_high[control->span];
				}
				else if (sound_mode != 4)
				{
					channels[0] = g_snd_user_data.pan_values[control->pan];
					channels[1] = g_snd_user_data.pan_values[pandiff];
					channels[2] = 0;
					channels[3] = 0;
					channels[4] = g_snd_user_data.pan_values[spandiff];
					channels[5] = g_snd_user_data.pan_values[control->span];
				}
				else
				{
					uint32 pan = 0x7F;
					if (((uint32)control->pan >> 1) < 0x7F)
						pan = (uint32)control->pan >> 1;

					channels[0] = g_snd_user_data.pan_values[pan] + g_snd_user_data.pan_values[spandiff];

					uint32 span = 0x7F;
					if (((uint32)pandiff >> 1) < 0x7E)
						span = (uint32)pandiff >> 1;

					channels[1] = g_snd_user_data.pan_values[span] + g_snd_user_data.pan_values[spandiff];

					// TODO
				}
			}
			else if (device_type == AX_DEV_DRC)
			{
				// TODO
			}
		}

		sint16 __MIXTranslateVolume(sint16 input)
		{
			if (input <= -904)
				return 0;

			if (input > 0x3C)
				return -156;

			return (sint16)g_snd_user_data.volume[input + 903];
		}

		void AXFXInitDefaultHooks();

		void MIXInit()
		{
			cemuLog_log(LogType::SoundAPI, "MIXInit()");

			if (g_snd_user_data.initialized)
				return;

			g_snd_user_data.max_voices = AX_MAX_VOICES; // AXGetMaxVoices();
			for (sint32 i = 0; i < g_snd_user_data.max_voices; ++i)
			{
				MIXResetChannelData(&g_snd_user_data.mix_channel[i]);
			}

			g_snd_user_data.initialized = true;
			g_snd_user_data.device_info.tv_sound_mode = 1;
			g_snd_user_data.device_info.drc_sound_mode = 1;
			g_snd_user_data.device_info.rmt_sound_mode = 0;

			AXFXInitDefaultHooks();
		}

		void MIXSetSoundMode(uint32 sound_mode)
		{
			cemuLog_log(LogType::SoundAPI, "MIXSetSoundMode(0x{:x})", sound_mode);

			if (sound_mode >= 2)
				sound_mode = 1;

			g_snd_user_data.device_info.tv_sound_mode = sound_mode;
		}

		uint32 MIXGetSoundMode()
		{
			cemuLog_log(LogType::SoundAPI, "MIXGetSoundMode()");
			return g_snd_user_data.device_info.tv_sound_mode;
		}

		void _MIXUpdateTV(MixChannel* channel, sint32 index)
		{
			assert(index == 0);

			bool updated_volume = false;
			if ((channel->tv_mode & AX_UPDATE_MODE_80000000) != 0)
			{
				for (size_t i = 0; i < AX_MAX_NUM_BUS; ++i)
				{
					for (size_t j = 0; j < AX_TV_CHANNEL_COUNT; ++j)
					{
						channel->tv_volume[i][j].volume = channel->tv_volume[i][j].volume_target;
					}
				}

				channel->tv_mode &= ~AX_UPDATE_MODE_80000000;
				updated_volume = true;
			}

			if ((channel->tv_mode & AX_UPDATE_MODE_40000000_VOLUME) == 0)
			{
				if (!updated_volume)
					return;
			}
			else
			{
				if (g_snd_user_data.device_info.tv_sound_mode == 0)
				{
					sint32 chan4 = channel->tv_channels[4];
					if (chan4 < -0x78)
						chan4 = -0x78;

					const sint32 fader = channel->tv_control.fader;

					channel->tv_volume[0][0].volume_target = __MIXTranslateVolume(chan4 + fader);
					channel->tv_volume[0][1].volume_target = __MIXTranslateVolume(chan4 + fader);
					channel->tv_volume[0][2].volume_target = 0;
					channel->tv_volume[0][3].volume_target = 0;
					channel->tv_volume[0][4].volume_target = 0;
					channel->tv_volume[0][5].volume_target = 0;

					for (int i = 0; i < 3; ++i)
					{
						const sint32 aux = channel->tv_control.aux[i];
						if ((channel->tv_mode & (1 << i)) == 0)
						{
							channel->tv_volume[1 + i][0].volume_target = __MIXTranslateVolume(chan4 + fader + aux);
							channel->tv_volume[1 + i][1].volume_target = __MIXTranslateVolume(chan4 + fader + aux);
						}
						else
						{
							channel->tv_volume[1 + i][0].volume_target = __MIXTranslateVolume(chan4 + aux);
							channel->tv_volume[1 + i][1].volume_target = __MIXTranslateVolume(chan4 + aux);
						}

						channel->tv_volume[1 + i][2].volume_target = 0;
						channel->tv_volume[1 + i][3].volume_target = 0;
						channel->tv_volume[1 + i][4].volume_target = 0;
						channel->tv_volume[1 + i][5].volume_target = 0;
					}

					channel->tv_mode &= ~AX_UPDATE_MODE_40000000_VOLUME;
					channel->tv_mode |= AX_UPDATE_MODE_80000000;
				}
				else if (g_snd_user_data.device_info.tv_sound_mode < 3)
				{
					sint32 chan4 = channel->tv_channels[4];
					if (chan4 < -0x78)
						chan4 = -0x78;

					const sint32 fader = channel->tv_control.fader;

					const sint32 chan0 = channel->tv_channels[0];
					const sint32 chan1 = channel->tv_channels[1];
					channel->tv_volume[0][0].volume_target = __MIXTranslateVolume(chan4 + chan0 + fader);
					channel->tv_volume[0][1].volume_target = __MIXTranslateVolume(chan4 + chan1 + fader);
					channel->tv_volume[0][2].volume_target = 0;
					channel->tv_volume[0][3].volume_target = 0;
					channel->tv_volume[0][4].volume_target = 0;
					channel->tv_volume[0][5].volume_target = 0;

					for (int i = 0; i < 3; ++i)
					{
						const sint32 aux = channel->tv_control.aux[i];
						if ((channel->tv_mode & (1 << i)) == 0)
						{
							channel->tv_volume[1 + i][0].volume_target = __MIXTranslateVolume(chan4 + chan0 + fader + aux);
							channel->tv_volume[1 + i][1].volume_target = __MIXTranslateVolume(chan4 + chan1 + fader + aux);
						}
						else
						{
							channel->tv_volume[1 + i][0].volume_target = __MIXTranslateVolume(chan4 + chan0 + aux);
							channel->tv_volume[1 + i][1].volume_target = __MIXTranslateVolume(chan4 + chan1 + aux);
						}

						channel->tv_volume[1 + i][2].volume_target = 0;
						channel->tv_volume[1 + i][3].volume_target = 0;
						channel->tv_volume[1 + i][4].volume_target = 0;
						channel->tv_volume[1 + i][5].volume_target = 0;
					}

					channel->tv_mode &= ~AX_UPDATE_MODE_40000000_VOLUME;
					channel->tv_mode |= AX_UPDATE_MODE_80000000;
				}
				else if (g_snd_user_data.device_info.tv_sound_mode == 3)
				{
					// TODO
				}
				else if (g_snd_user_data.device_info.tv_sound_mode == 4)
				{
					// TODO
				}
				else
				{
					channel->tv_mode &= ~AX_UPDATE_MODE_40000000_VOLUME;
					channel->tv_mode |= AX_UPDATE_MODE_80000000;
				}
			}

			AXCHMIX2 mix[AX_TV_CHANNEL_COUNT][AX_MAX_NUM_BUS];
			for (size_t i = 0; i < AX_MAX_NUM_BUS; ++i)
			{
				for (size_t j = 0; j < AX_TV_CHANNEL_COUNT; ++j)
				{
					const sint16 target = channel->tv_volume[i][j].volume_target;
					const sint16 volume = channel->tv_volume[i][j].volume;

					mix[j][i].vol = volume;
					mix[j][i].delta = (target - volume) / 96; // 32000HZ SAMPLES_3MS
				}
			}
			AXSetVoiceDeviceMix(channel->voice.GetPtr(), AX_DEV_TV, index, (snd_core::AXCHMIX_DEPR*)&mix[0][0]);
		}

		void _MIXUpdateDRC(MixChannel* channel, sint32 index)
		{
			// todo
		}

		void _MIXUpdateRmt(MixChannel* channel, sint32 index)
		{
			// todo
		}

		void MIXInitChannel(AXVPB* voice, uint16 mode, uint16 input, uint16 aux1, uint16 aux2, uint16 aux3, uint16 pan, uint16 span, uint16 fader)
		{
			cemuLog_log(LogType::SoundAPI, "MIXInitChannel(0x{:x}, 0x{:x}, 0x{:x}, 0x{:x}, 0x{:x}, 0x{:x}, 0x{:x}, 0x{:x}, 0x{:x})", MEMPTR(voice).GetMPTR(), mode, input, aux1, aux2, aux3, pan, span, fader);
			cemu_assert_debug(voice);

			AXVoiceBegin(voice);

			MIXAssignChannel(voice);
			MIXInitInputControl(voice, input, mode);

			const uint32 index = voice->index;
			auto& channel = g_snd_user_data.mix_channel[index];

			channel.tv_control.aux[0] = aux1;
			channel.tv_control.aux[1] = aux2;
			channel.tv_control.aux[2] = aux3;

			channel.tv_control.pan = pan;
			channel.tv_control.span = span;
			channel.tv_control.fader = fader;
			// channel.tv_control.lfe = lfe; // 0x1A -> not set?

			channel.tv_mode = AX_UPDATE_MODE_40000007 & mode;
			_MIXControl_SetDevicePan(&channel.tv_control, AX_DEV_TV, channel.tv_channels);

			channel.tv_mode |= AX_UPDATE_MODE_40000000_VOLUME;
			_MIXUpdateTV(&channel, 0);

			AXVoiceEnd(voice);
		}

		void MIXAssignChannel(AXVPB* voice)
		{
			cemuLog_log(LogType::SoundAPI, "MIXAssignChannel(0x{:x})", MEMPTR(voice).GetMPTR());
			cemu_assert_debug(voice);

			AXVoiceBegin(voice);

			const uint32 voice_index = voice->index;
			auto channel = &g_snd_user_data.mix_channel[voice_index];
			MIXResetChannelData(channel);
			channel->voice = voice;

			AXVoiceEnd(voice);
		}

		void MIXDRCInitChannel(AXVPB* voice, uint16 mode, uint16 vol1, uint16 vol2, uint16 vol3)
		{
			cemuLog_log(LogType::SoundAPI, "MIXDRCInitChannel(0x{:x}, 0x{:x}, 0x{:x}, 0x{:x}, 0x{:x})", MEMPTR(voice).GetMPTR(), mode, vol1, vol2, vol3);
			cemu_assert_debug(voice);

			AXVoiceBegin(voice);

			const uint32 index = voice->index;
			auto& channel = g_snd_user_data.mix_channel[index];

			_MIXChannelResetDRC(&channel, 0);

			channel.drc_volume[1][1][1].volume = vol1;
			channel.drc_volume[1][1][2].volume_target = vol2;
			channel.drc_volume[1][1][3].volume_target = vol3;

			channel.drc_mode[0] = AX_UPDATE_MODE_40000007 & mode;
			_MIXControl_SetDevicePan(&channel.drc_control[0], AX_DEV_DRC, &channel.drc_channels[0][0]);

			channel.drc_mode[0] |= AX_UPDATE_MODE_40000000_VOLUME;
			_MIXUpdateDRC(&channel, 0);

			AXVoiceEnd(voice);
		}

		void MIXSetInput(AXVPB* voice, uint16 input)
		{
			cemuLog_log(LogType::SoundAPI, "MIXSetInput(0x{:x}, 0x{:x})", MEMPTR(voice).GetMPTR(), input);

			const uint32 voice_index = voice->index;
			const auto channel = &g_snd_user_data.mix_channel[voice_index];

			channel->input_level = input;
			channel->update_mode |= AX_UPDATE_MODE_10000000;

		}

		void MIXUpdateSettings()
		{
			cemuLog_log(LogType::SoundAPI, "MIXUpdateSettings()");

			if (!g_snd_user_data.initialized)
				return;
			
			if (g_snd_user_data.max_voices <= 0)
				return;

			for (sint32 i = 0; i < g_snd_user_data.max_voices; ++i)
			{
				auto& channel = g_snd_user_data.mix_channel[i];
				if (!channel.voice)
					continue;

				const auto voice = channel.voice.GetPtr();

				AXVoiceBegin(voice);

				bool volume_updated = false;
				if ((channel.update_mode & AX_UPDATE_MODE_20000000) != 0)
				{
					channel.volume.volume = channel.volume.volume_target;
					channel.update_mode &= ~AX_UPDATE_MODE_20000000;
					volume_updated = true;
				}

				if ((channel.update_mode & AX_UPDATE_MODE_10000000) == 0)
				{
					if (volume_updated)
					{
						AXPBVE ve;
						ve.currentVolume = channel.volume.volume;
						ve.currentDelta = (channel.volume.volume_target - channel.volume.volume) / 96;
						AXSetVoiceVe(voice, &ve);
					}
				}
				else
				{
					sint32 volume = 0;
					if ((channel.update_mode & 8) == 0)
						volume = __MIXTranslateVolume(channel.input_level);

					channel.volume.volume_target = volume;

					channel.update_mode &= ~AX_UPDATE_MODE_10000000;
					channel.update_mode |= AX_UPDATE_MODE_20000000;

					AXPBVE ve;
					ve.currentVolume = channel.volume.volume;
					ve.currentDelta = (channel.volume.volume_target - channel.volume.volume) / 96;
					AXSetVoiceVe(voice, &ve);
				}

				_MIXUpdateTV(&channel, 0);

				for (int i = 0; i < 2; ++i)
					_MIXUpdateDRC(&channel, i);

				// TODO remote mix
				AXCHMIX2 mix[4];
				for (int j = 0; j < 4; ++j)
				{
					AXSetVoiceDeviceMix(voice, AX_DEV_RMT, i, (snd_core::AXCHMIX_DEPR*)mix);
				}


				AXVoiceEnd(voice);
			}

			// TODO
		}

		void MIXSetDeviceSoundMode(uint32 device, uint32 mode)
		{
			cemuLog_log(LogType::SoundAPI, "MIXSetDeviceSoundMode(0x{:x}, 0x{:x})", device, mode);
			cemu_assert_debug(device < AX_DEV_COUNT);
			cemu_assert_debug(mode <= 4);

			bool is_tv_device = false;
			bool is_drc_device = false;
			if (device == AX_DEV_TV)
			{
				g_snd_user_data.device_info.tv_sound_mode = mode;
				is_tv_device = true;
			}
			else if (device == AX_DEV_DRC)
			{
				cemu_assert_debug(mode <= 2);
				g_snd_user_data.device_info.drc_sound_mode = mode;
				is_drc_device = true;
			}
			else if (device == AX_DEV_RMT)
			{
				cemu_assert_debug(mode == 0);
				g_snd_user_data.device_info.rmt_sound_mode = mode;
			}
			else
			{
				cemuLog_log(LogType::SoundAPI, "ERROR: MIXSetDeviceSoundMode(0x{:x}, 0x{:x}) -> wrong device", device, mode);
			}

			for (sint32 i = 0; i < g_snd_user_data.max_voices; ++i)
			{
				auto& channel = g_snd_user_data.mix_channel[i];
				if (!channel.voice)
					continue;

				const auto voice = channel.voice.GetPtr();
				AXVoiceBegin(voice);

				if (is_tv_device)
				{
					channel.tv_mode |= AX_UPDATE_MODE_40000000_VOLUME;
					_MIXControl_SetDevicePan(&channel.tv_control, AX_DEV_TV, channel.tv_channels);
				}

				if (is_drc_device)
				{
					for (sint32 j = 0; j < AX_MAX_NUM_DRC; ++j)
					{
						channel.drc_mode[j] |= AX_UPDATE_MODE_40000000_VOLUME;
						_MIXControl_SetDevicePan(&channel.drc_control[j], AX_DEV_DRC, channel.drc_channels[j]);
					}
				}

				AXVoiceEnd(voice);
			}
		}

		void MIXInitDeviceControl(AXVPB* voice, uint32 device_type, uint32 index, MixControl* control, uint32 mode)
		{
			cemuLog_log(LogType::SoundAPI, "MIXInitDeviceControl(0x{:0x}, 0x{:x}, 0x{:x}, 0x{:x}, 0x{:x} )", MEMPTR(voice).GetMPTR(), device_type, index, MEMPTR(control).GetMPTR(), mode);

			cemu_assert_debug(device_type < AX_DEV_COUNT);
			cemu_assert_debug(voice);
			cemu_assert_debug(control);

			AXVoiceBegin(voice);

			const uint32 voice_index = voice->index;
			auto& channel = g_snd_user_data.mix_channel[voice_index];

			channel.voice = voice;

			if (device_type == AX_DEV_TV)
			{
				cemu_assert_debug(index == 0);
				_MIXChannelResetTV(&channel, index);

				memcpy(&channel.tv_control, control, sizeof(MixControl));
				_MIXControl_SetDevicePan(&channel.tv_control, device_type, channel.tv_channels);

				channel.tv_mode |= AX_UPDATE_MODE_40000000_VOLUME;
				_MIXUpdateTV(&channel, index);
			}
			else if (device_type == AX_DEV_DRC)
			{
				cemu_assert_debug(index < 2);
				_MIXChannelResetDRC(&channel, index);

				memcpy(&channel.drc_control[index], control, sizeof(MixControl));
				_MIXControl_SetDevicePan(&channel.drc_control[index], device_type, channel.drc_channels[index]);

				channel.drc_mode[index] |= AX_UPDATE_MODE_40000000_VOLUME;
				_MIXUpdateDRC(&channel, index);
			}
			else if (device_type == AX_DEV_RMT)
			{
				cemu_assert_debug(index < 4);
				_MIXChannelResetRmt(&channel, index);

				memcpy(&channel.rmt_control[index], control, sizeof(MixControl));
				_MIXControl_SetDevicePan(&channel.rmt_control[index], device_type, channel.rmt_channels[index]);

				channel.rmt_mode[index] = mode & 0xf;
				_MIXUpdateRmt(&channel, index);
			}

			AXVoiceEnd(voice);
		}


		void MIXInitInputControl(AXVPB* voice, uint16 input, uint32 mode)
		{
			cemuLog_log(LogType::SoundAPI, "MIXInitInputControl(0x{:x}, 0x{:x}, 0x{:x} )", MEMPTR(voice).GetMPTR(), input, mode);
			cemu_assert_debug(voice);

			AXVoiceBegin(voice);

			const uint32 voice_index = voice->index;
			auto& channel = g_snd_user_data.mix_channel[voice_index];

			mode &= 8;
			mode |= AX_UPDATE_MODE_10000000;
			channel.update_mode = mode;

			channel.input_level = input;

			AXVoiceEnd(voice);
		}

		void MIXSetDeviceFader(AXVPB* vpb, uint32 device, uint32 deviceIndex, sint16 newFader)
		{
			// not well tested
			cemu_assert(device < AX_DEV_COUNT);
			MixChannel& mixChannel = g_snd_user_data.mix_channel[vpb->index];

			AXVoiceBegin(vpb);

			MixControl& mixControl = mixChannel.GetMixControl(device, deviceIndex);
			MixMode& mixMode = mixChannel.GetMode(device, deviceIndex);

			if (mixControl.fader == newFader)
			{
				AXVoiceEnd(vpb);
				return;
			}

			mixControl.fader = newFader;
			mixMode |= AX_UPDATE_MODE_40000000_VOLUME;
			AXVoiceEnd(vpb);
		}

		void MIXSetDevicePan(AXVPB* vpb, uint32 device, uint32 deviceIndex, sint16 newPan)
		{
			// not well tested
			cemu_assert(device < AX_DEV_COUNT);
			MixChannel& mixChannel = g_snd_user_data.mix_channel[vpb->index];

			AXVoiceBegin(vpb);

			MixControl& mixControl = mixChannel.GetMixControl(device, deviceIndex);
			MixMode& mixMode = mixChannel.GetMode(device, deviceIndex);
			sint16* deviceChannels = mixChannel.GetChannels(device, deviceIndex);

			if (mixControl.fader == newPan)
			{
				AXVoiceEnd(vpb);
				return;
			}

			mixControl.pan = newPan;
			_MIXControl_SetDevicePan(&mixControl, device, deviceChannels);
			mixMode |= AX_UPDATE_MODE_40000000_VOLUME;
			AXVoiceEnd(vpb);
		}

		struct AXPBADPCM
		{
			uint16be table[8][2]; // 0x00
			uint16be gain;
			uint16be predicator;
			uint16be yn1;
			uint16be yn2;
		};

		struct AXPBADPCMLOOP
		{
			uint16be predicator; // 0x00
			uint16be yn1; // 0x02
			uint16be yn2; // 0x04
		};

		struct SPAdpcmEntry
		{
			AXPBADPCM adpcm;
			AXPBADPCMLOOP adpcmLoop;
		};

		struct SPSoundEntry
		{
			uint32be type; // 0x00
			uint32be sampleRate; // 0x04
			uint32be loopAddress; // 0x08
			uint32be loopEndAddress; // 0x0C
			uint32be endOffset; // 0x10
			uint32be currentOffset; // 0x14
			MEMPTR<SPAdpcmEntry> adpcmEntry; // 0x18
		};
		static_assert(sizeof(SPSoundEntry) == 0x1C);

		struct SPSoundTable
		{
			uint32be count;
			SPSoundEntry entries[1];
		};

		void SPInitSoundTable(SPSoundTable* soundTable, uint8* samples, uint32be* endOffsetPtr)
		{
			cemuLog_log(LogType::SoundAPI, "SPInitSoundTable(0x{:x}, 0x{:x}, 0x{:x} )", MEMPTR(soundTable).GetMPTR(), MEMPTR(samples).GetMPTR(), MEMPTR(endOffsetPtr).GetMPTR());
			if (!soundTable)
				return;

			if (!samples)
				return;

			uint32be endOffset = 0;
			for(uint32 i = 0; i < soundTable->count; ++i)
			{
				auto& entryPtr = soundTable->entries[i];
				if(entryPtr.type == 0)
				{
					entryPtr.loopAddress = entryPtr.currentOffset;
					entryPtr.loopEndAddress = 0;
					uint32be tmp = entryPtr.endOffset >> 1;
					if (tmp > endOffset)
						endOffset = tmp;
					/// ???
				}
				else if (entryPtr.type == 1)
				{
					uint32be tmp = entryPtr.endOffset >> 1;
					if (tmp > endOffset)
						endOffset = tmp;
					/// ???
				}
				else if (entryPtr.type == 2)
				{
					entryPtr.loopAddress = entryPtr.currentOffset;
					entryPtr.loopEndAddress = 0;
					uint32be tmp = entryPtr.endOffset << 1;
					if (tmp > endOffset)
						endOffset = tmp;
				}
				else if (entryPtr.type == 3)
				{
					uint32be tmp = entryPtr.endOffset << 1;
					if (tmp > endOffset)
						endOffset = tmp;
				}
				else if (entryPtr.type == 4)
				{
					entryPtr.loopAddress = entryPtr.currentOffset;
					entryPtr.loopEndAddress = 0;

					if (entryPtr.endOffset > endOffset)
						endOffset = entryPtr.endOffset;					
				}
				else if (entryPtr.type == 5)
				{
					if (entryPtr.endOffset > endOffset)
						endOffset = entryPtr.endOffset;
				}
			}
			

			if (endOffsetPtr)
				*endOffsetPtr = endOffset;
		
		}

		SPSoundEntry* SPGetSoundEntry(SPSoundTable* soundTable, uint32 index)
		{
			cemuLog_log(LogType::SoundAPI, "SPGetSoundEntry(0x{:x}, {})", MEMPTR(soundTable).GetMPTR(), index);
			if (!soundTable)
				return nullptr;

			if (soundTable->count <= index)
				return nullptr;

			return &soundTable->entries[index];
		}

		MEMPTR<void> s_fxAlloc = nullptr;
		MEMPTR<void> s_fxFree = nullptr;

		void _AXDefaultHook_alloc(PPCInterpreter_t* hCPU)
		{
			uint32 size = hCPU->gpr[3];
			MEMPTR<void> mem = coreinit::_weak_MEMAllocFromDefaultHeap(size);
			osLib_returnFromFunction(hCPU, mem.GetMPTR());
		}

		void _AXDefaultHook_free(PPCInterpreter_t* hCPU)
		{
			MEMPTR<void> mem{ hCPU->gpr[3] };
			return coreinit::_weak_MEMFreeToDefaultHeap(mem.GetPtr());
		}

		void AXFXInitDefaultHooks()
		{
			// todo - this should only be applied when the library is loaded? MIXInit() does not affect this?
			if (!s_fxAlloc)
			{
				s_fxAlloc = RPLLoader_MakePPCCallable(_AXDefaultHook_alloc);
				s_fxFree = RPLLoader_MakePPCCallable(_AXDefaultHook_free);
			}
		}

		void AXFXSetHooks(void* allocFunc, void* freeFunc)
		{
			s_fxAlloc = allocFunc;
			s_fxFree = freeFunc;
		}

		void AXFXGetHooks(MEMPTR<void>* allocFuncOut, MEMPTR<void>* freeFuncOut)
		{
			*allocFuncOut = s_fxAlloc;
			*freeFuncOut = s_fxFree;
		}

		void* AXFXInternalAlloc(uint32 size, bool clearToZero)
		{
			if (s_fxAlloc)
			{
				MEMPTR<void> mem{ PPCCoreCallback(s_fxAlloc, size) };
				if (clearToZero)
					memset(mem.GetPtr(), 0, size);
				return mem.GetPtr();
			}
			void* mem = coreinit::_weak_MEMAllocFromDefaultHeapEx(size, 4);
			if (clearToZero)
				memset(mem, 0, size);
			return mem;
		}

		void AXFXInternalFree(void* mem)
		{
			if (s_fxFree)
			{
				PPCCoreCallback(s_fxFree, mem);
				return;
			}
			coreinit::_weak_MEMFreeToDefaultHeap(mem);
		}

		/* AUX callback */

		struct AUXCBSAMPLEDATA
		{
			MEMPTR<sint32be> channelSamples[6];
		};

		bool gUnsupportedSoundEffectWarning = false;
		void PrintUnsupportedSoundEffectWarning()
		{
			if (gUnsupportedSoundEffectWarning)
				return;
			cemuLog_log(LogType::Force, "The currently running title is trying to utilize an unsupported audio effect");
			cemuLog_log(LogType::Force, "To emulate these correctly, place snd_user.rpl and snduser2.rpl from the original Wii U firmware in /cafeLibs/ folder");
			gUnsupportedSoundEffectWarning = true;
		}

		void __UnimplementedFXCallback(AUXCBSAMPLEDATA* auxSamples, size_t sampleCount, bool clearCh0, bool clearCh1, bool clearCh2, bool clearCh3, bool clearCh4, bool clearCh5)
		{
			PrintUnsupportedSoundEffectWarning();
			bool clearChannel[6] = { clearCh0, clearCh1, clearCh2, clearCh3, clearCh4, clearCh5 };

			for (sint32 channel = 0; channel < 6; channel++)
			{
				if(!clearChannel[channel])
					continue;
				sint32be* channelPtr = auxSamples->channelSamples[channel].GetPtr();
				while (sampleCount)
				{
					*channelPtr = 0;
					channelPtr++;
					sampleCount--;
				}
			}

		}

		/* AXFXReverbHi */

		struct AXFXReverbHiData
		{
			// todo - implement
			uint32be placeholder;
		};

		void AXFXReverbHiInit(AXFXReverbHiData* param)
		{
			cemuLog_log(LogType::Force, "AXFXReverbHiInit - stub");
		}

		void AXFXReverbHiSettings(AXFXReverbHiData* param)
		{
			cemuLog_log(LogType::Force, "AXFXReverbHiSettings - stub");
		}

		void AXFXReverbHiShutdown(AXFXReverbHiData* param)
		{
			cemuLog_log(LogType::Force, "AXFXReverbHiShutdown - stub");
		}

		void AXFXReverbHiCallback(AUXCBSAMPLEDATA* auxSamples, AXFXReverbHiData* reverbHi)
		{
			// todo - implement me
			__UnimplementedFXCallback(auxSamples, 96, true, true, true, false, false, false);
		}

		/* AXFXMultiChReverb */

		struct AXFXMultiChReverbData
		{
			// todo - implement
			uint32 placeholder;
		};

		void AXFXMultiChReverbInit(AXFXMultiChReverbData* param, int ukn2, int ukn3)
		{
			cemuLog_log(LogType::Force, "AXFXMultiChReverbInit (Stubbed)");
		}

		void AXFXMultiChReverbSettingsUpdate(AXFXMultiChReverbData* param)
		{
			// todo
		}

		void AXFXMultiChReverbShutdown(AXFXMultiChReverbData* param)
		{
			// todo
		}

		void AXFXMultiChReverbCallback(AUXCBSAMPLEDATA* auxSamples, AXFXMultiChReverbData* reverbHi, AXAUXCBCHANNELINFO* auxInfo)
		{
			// todo - implement me
			__UnimplementedFXCallback(auxSamples, auxInfo->numSamples, true, true, true, true, true, true);
		}

		/* AXFXReverbStd */

		struct AXFXReverbStdData
		{
			uint32be placeholder;
		};

		uint32 AXFXReverbStdExpGetMemSize(AXFXReverbStdData* reverbParam)
		{
			return 0xC; // placeholder size
		}

		bool AXFXReverbStdExpInit(AXFXReverbStdData* reverbParam)
		{
			return true;
		}

		void AXFXReverbStdExpShutdown(AXFXReverbStdData* reverbParam)
		{

		}

		void AXFXReverbStdExpCallback(AUXCBSAMPLEDATA* auxSamples, AXFXReverbStdData* reverbData)
		{
			// todo - implement me
			__UnimplementedFXCallback(auxSamples, 96, true, true, true, false, false, false);
		}

		void Initialize()
		{
			/* snd_user */
			cafeExportRegister("snd_user", MIXInit, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXSetSoundMode, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXGetSoundMode, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXInitChannel, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXAssignChannel, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXDRCInitChannel, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXSetInput, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXUpdateSettings, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXSetDeviceSoundMode, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXSetDeviceFader, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXSetDevicePan, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXInitDeviceControl, LogType::SoundAPI);
			cafeExportRegister("snd_user", MIXInitInputControl, LogType::SoundAPI);

			

			cafeExportRegister("snd_user", SPInitSoundTable, LogType::SoundAPI);
			cafeExportRegister("snd_user", SPGetSoundEntry, LogType::SoundAPI);
			//cafeExportRegister("snd_user", SPPrepareSound);
			cafeExportRegister("snd_user", AXFXSetHooks, LogType::SoundAPI);
			cafeExportRegister("snd_user", AXFXGetHooks, LogType::SoundAPI);

			cafeExportRegister("snd_user", AXFXReverbHiInit, LogType::SoundAPI);
			cafeExportRegister("snd_user", AXFXReverbHiSettings, LogType::SoundAPI);
			cafeExportRegister("snd_user", AXFXReverbHiShutdown, LogType::SoundAPI);
			cafeExportRegister("snd_user", AXFXReverbHiCallback, LogType::SoundAPI);

			cafeExportRegister("snd_user", AXFXMultiChReverbInit, LogType::SoundAPI);
			cafeExportRegister("snd_user", AXFXMultiChReverbSettingsUpdate, LogType::SoundAPI);
			cafeExportRegister("snd_user", AXFXMultiChReverbShutdown, LogType::SoundAPI);
			cafeExportRegister("snd_user", AXFXMultiChReverbCallback, LogType::SoundAPI);

			/* snduser2 */
			cafeExportRegister("snduser2", MIXInit, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXSetSoundMode, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXGetSoundMode, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXInitChannel, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXAssignChannel, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXDRCInitChannel, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXSetInput, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXUpdateSettings, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXSetDeviceSoundMode, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXSetDeviceFader, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXSetDevicePan, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXInitDeviceControl, LogType::SoundAPI);
			cafeExportRegister("snduser2", MIXInitInputControl, LogType::SoundAPI);

			cafeExportRegister("snduser2", AXFXSetHooks, LogType::Placeholder);
			cafeExportRegister("snduser2", AXFXGetHooks, LogType::Placeholder);
			cafeExportRegister("snduser2", AXFXReverbStdExpGetMemSize, LogType::Placeholder);
			cafeExportRegister("snduser2", AXFXReverbStdExpInit, LogType::Placeholder);
			cafeExportRegister("snduser2", AXFXReverbStdExpShutdown, LogType::Placeholder);
			cafeExportRegister("snduser2", AXFXReverbStdExpCallback, LogType::Placeholder);

			cafeExportRegister("snduser2", AXFXReverbHiInit, LogType::SoundAPI);
			cafeExportRegister("snduser2", AXFXReverbHiSettings, LogType::SoundAPI);
			cafeExportRegister("snduser2", AXFXReverbHiShutdown, LogType::SoundAPI);
			cafeExportRegister("snduser2", AXFXReverbHiCallback, LogType::SoundAPI);

			cafeExportRegister("snduser2", AXFXMultiChReverbInit, LogType::SoundAPI);
			cafeExportRegister("snduser2", AXFXMultiChReverbSettingsUpdate, LogType::SoundAPI);
			cafeExportRegister("snduser2", AXFXMultiChReverbShutdown, LogType::SoundAPI);
			cafeExportRegister("snduser2", AXFXMultiChReverbCallback, LogType::SoundAPI);
		}
	}
}
