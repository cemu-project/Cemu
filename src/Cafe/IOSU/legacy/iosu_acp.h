#pragma once

#include "Cafe/IOSU/iosu_types_common.h"
#include "Cafe/OS/libs/nn_common.h" // for nnResult

typedef struct
{
	/* +0x0000 */ uint64 title_id; // parsed via GetHex64
	/* +0x0008 */ uint64 boss_id; // parsed via GetHex64
	/* +0x0010 */ uint64 os_version; // parsed via GetHex64
	/* +0x0018 */ uint64 app_size; // parsed via GetHex64
	/* +0x0020 */ uint64 common_save_size; // parsed via GetHex64
	/* +0x0028 */ uint64 account_save_size; // parsed via GetHex64
	/* +0x0030 */ uint64 common_boss_size; // parsed via GetHex64
	/* +0x0038 */ uint64 account_boss_size; // parsed via GetHex64
	/* +0x0040 */ uint64 join_game_mode_mask; // parsed via GetHex64
	/* +0x0048 */ uint32be version;
	/* +0x004C */ char product_code[0x20];
	/* +0x006C */ char content_platform[0x20];
	/* +0x008C */ char company_code[8];
	/* +0x0094 */ char mastering_date[0x20];
	/* +0x00B4 */ uint32be logo_type;
	/* +0x00B8 */ uint32be app_launch_type;
	/* +0x00BC */ uint32be invisible_flag;
	/* +0x00C0 */ uint32be no_managed_flag;
	/* +0x00C4 */ uint32be no_event_log;
	/* +0x00C8 */ uint32be no_icon_database;
	/* +0x00CC */ uint32be launching_flag;
	/* +0x00D0 */ uint32be install_flag;
	/* +0x00D4 */ uint32be closing_msg;
	/* +0x00D8 */ uint32be title_version;
	/* +0x00DC */ uint32be group_id; // Hex32
	/* +0x00E0 */ uint32be save_no_rollback;
	/* +0x00E4 */ uint32be bg_daemon_enable;
	/* +0x00E8 */ uint32be join_game_id; // Hex32
	/* +0x00EC */ uint32be olv_accesskey;
	/* +0x00F0 */ uint32be wood_tin;
	/* +0x00F4 */ uint32be e_manual;
	/* +0x00F8 */ uint32be e_manual_version;
	/* +0x00FC */ uint32be region; // Hex32
	/* +0x0100 */ uint32be pc_cero;
	/* +0x0104 */ uint32be pc_esrb;
	/* +0x0108 */ uint32be pc_bbfc;
	/* +0x010C */ uint32be pc_usk;
	/* +0x0110 */ uint32be pc_pegi_gen;
	/* +0x0114 */ uint32be pc_pegi_fin;
	/* +0x0118 */ uint32be pc_pegi_prt;
	/* +0x011C */ uint32be pc_pegi_bbfc;
	/* +0x0120 */ uint32be pc_cob;
	/* +0x0124 */ uint32be pc_grb;
	/* +0x0128 */ uint32be pc_cgsrr;
	/* +0x012C */ uint32be pc_oflc;
	/* +0x0130 */ uint32be pc_reserved0;
	/* +0x0134 */ uint32be pc_reserved1;
	/* +0x0138 */ uint32be pc_reserved2;
	/* +0x013C */ uint32be pc_reserved3;
	/* +0x0140 */ uint32be ext_dev_nunchaku;
	/* +0x0144 */ uint32be ext_dev_classic;
	/* +0x0148 */ uint32be ext_dev_urcc;
	/* +0x014C */ uint32be ext_dev_board;
	/* +0x0150 */ uint32be ext_dev_usb_keyboard;
	/* +0x0154 */ uint32be ext_dev_etc;
	/* +0x0158 */ char ext_dev_etc_name[0x200];
	/* +0x0358 */ uint32be eula_version;
	/* +0x035C */ uint32be drc_use;
	/* +0x0360 */ uint32be network_use;
	/* +0x0364 */ uint32be online_account_use;
	/* +0x0368 */ uint32be direct_boot;
	/* +0x036C */ uint32be reserved_flag[8]; // array of U32, reserved_flag%d
	/* +0x038C */ char     longname_ja[0x200];
	/* +0x058C */ char     longname_en[0x200];
	/* +0x078C */ char     longname_fr[0x200];
	/* +0x098C */ char     longname_de[0x200];
	/* +0x0B8C */ char     longname_it[0x200];
	/* +0x0D8C */ char     longname_es[0x200];
	/* +0x0F8C */ char     longname_zhs[0x200];
	/* +0x118C */ char     longname_ko[0x200];
	/* +0x138C */ char     longname_nl[0x200];
	/* +0x158C */ char     longname_pt[0x200];
	/* +0x178C */ char     longname_ru[0x200];
	/* +0x198C */ char     longname_zht[0x200];
	/* +0x1B8C */ char     shortname_ja[0x100];
	/* +0x1C8C */ char     shortname_en[0x100];
	/* +0x1D8C */ char     shortname_fr[0x100];
	/* +0x1E8C */ char     shortname_de[0x100];
	/* +0x1F8C */ char     shortname_it[0x100];
	/* +0x208C */ char     shortname_es[0x100];
	/* +0x218C */ char     shortname_zhs[0x100];
	/* +0x228C */ char     shortname_ko[0x100];
	/* +0x238C */ char     shortname_nl[0x100];
	/* +0x248C */ char     shortname_pt[0x100];
	/* +0x258C */ char     shortname_ru[0x100];
	/* +0x268C */ char     shortname_zht[0x100];
	/* +0x278C */ char     publisher_ja[0x100];
	/* +0x288C */ char     publisher_en[0x100];
	/* +0x298C */ char     publisher_fr[0x100];
	/* +0x2A8C */ char     publisher_de[0x100];
	/* +0x2B8C */ char     publisher_it[0x100];
	/* +0x2C8C */ char     publisher_es[0x100];
	/* +0x2D8C */ char     publisher_zhs[0x100];
	/* +0x2E8C */ char     publisher_ko[0x100];
	/* +0x2F8C */ char     publisher_nl[0x100];
	/* +0x308C */ char     publisher_pt[0x100];
	/* +0x318C */ char     publisher_ru[0x100];
	/* +0x328C */ char     publisher_zht[0x100];
	/* +0x338C */ uint32be add_on_unique_id[0x20]; // Hex32, add_on_unique_id%d
	/* +0x340C */ uint8    padding[0x3440 - 0x340C]; // guessed
													 // struct size is 0x3440
}acpMetaXml_t;

typedef struct
{
	/* +0x06BDC | +0x00000 */ uint8 bootMovie[0x13B38];
	/* +0x1A714 | +0x13B38 */ uint8 bootLogoTex[0x6FBC];
	/* +0x216D0 | +0x1AAF4 */ uint8 ukn1AAF4[4];
	/* +0x216D4 | +0x1AAF8 */ uint8 ukn1AAF8[4];
	/* +0x216D8 | +0x1AAFC */ uint8 ukn1AAFC[4];
}acpMetaData_t;

typedef struct
{
	uint32be titleIdHigh;
	uint32be titleIdLow;
}acpTitleId_t;

typedef struct
{
	// for ACPGetTitleSaveDirEx

	uint32be ukn00;
	uint32be ukn04;
	uint32be persistentId;
	uint32be ukn0C;
	//uint32be ukn10; // ukn10/ukn14 are part of size
	//uint32be ukn14;
	//uint32be ukn18; // ukn18/ukn1C are part of size
	//uint32be ukn1C;
	uint64 sizeA;
	uint64 sizeB;
	// path starts at 0x20, length unknown?
	char path[0x40]; // %susr/save/%08x/%08x/meta/			// /vol/storage_mlc01/
	/* +0x60 */ uint64 time;
	/* +0x68 */ uint8 padding[0x80 - 0x68];
	// size is 0x80, but actual content size is only 0x60 and padded to 0x80?
}acpSaveDirInfo_t;


// custom dev/acp_main protocol (Cemu only)
#define IOSU_ACP_REQUEST_CEMU		(0xEE)

typedef struct
{
	uint32 requestCode;
	// input
	uint8 accountSlot;
	//uint32 unique;
	//uint64 titleId;
	//uint32 titleVersion;
	//uint32 serverId;
	uint64 titleId;
	sint32 type;
	MEMPTR<void> ptr;
	sint32 maxCount;
	// output
	uint32 returnCode; // ACP return value
	union
	{
		//struct
		//{
		//	uint64 u64;
		//}resultU64;
		struct
		{
			uint32 u32;
		}resultU32;
		//struct
		//{
		//	char strBuffer[1024];
		//}resultString;
		//struct
		//{
		//	uint8 binBuffer[1024];
		//}resultBinary;
	};
}iosuAcpCemuRequest_t;

// ACP request Cemu subcodes
#define IOSU_ACP_GET_SAVE_DATA_TITLE_ID_LIST				0x01
#define IOSU_ACP_GET_TITLE_SAVE_META_XML					0x02
#define IOSU_ACP_GET_TITLE_SAVE_DIR							0x03
#define IOSU_ACP_GET_TITLE_META_DATA						0x04
#define IOSU_ACP_GET_TITLE_META_XML							0x05
#define IOSU_ACP_CREATE_SAVE_DIR_EX							0x06

namespace iosu
{
	void iosuAcp_init();

	namespace acp
	{
		enum ACPDeviceType
		{
			UnknownType = 0,
			InternalDeviceType = 1,
			USBDeviceType = 3,
		};

		class IOSUModule* GetModule();

		void CreateSaveMetaFiles(uint32 persistentId, uint64 titleId);
		nnResult ACPUpdateSaveTimeStamp(uint32 persistentId, uint64 titleId, ACPDeviceType deviceType);

		nnResult ACPCreateSaveDir(uint32 persistentId, ACPDeviceType type);
		sint32 ACPCreateSaveDirEx(uint8 accountSlot, uint64 titleId);
		nnResult ACPGetOlvAccesskey(uint32be* accessKey);
	}

}