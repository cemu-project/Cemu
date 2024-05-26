#include "iosu_ioctl.h"
#include "iosu_acp.h"
#include "Cafe/OS/libs/nn_common.h"
#include "util/tinyxml2/tinyxml2.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/nn_save/nn_save.h"
#include "util/helpers/helpers.h"
#include "Cafe/OS/libs/nn_acp/nn_acp.h"
#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/Filesystem/fsc.h"
//#include "Cafe/HW/Espresso/PPCState.h"

#include "Cafe/IOSU/iosu_types_common.h"
#include "Cafe/IOSU/nn/iosu_nn_service.h"

#include "Cafe/IOSU/legacy/iosu_act.h"
#include "Cafe/CafeSystem.h"
#include "config/ActiveSettings.h"

#include <inttypes.h>

using ACPDeviceType = iosu::acp::ACPDeviceType;

static_assert(sizeof(acpMetaXml_t) == 0x3440);
static_assert(offsetof(acpMetaXml_t, title_id) == 0x0000);
static_assert(offsetof(acpMetaXml_t, boss_id) == 0x0008);
static_assert(offsetof(acpMetaXml_t, os_version) == 0x0010);
static_assert(offsetof(acpMetaXml_t, app_size) == 0x0018);
static_assert(offsetof(acpMetaXml_t, common_save_size) == 0x0020);

static_assert(offsetof(acpMetaXml_t, version) == 0x0048);
static_assert(offsetof(acpMetaXml_t, product_code) == 0x004C);
static_assert(offsetof(acpMetaXml_t, logo_type) == 0x00B4);
static_assert(offsetof(acpMetaXml_t, pc_cero) == 0x0100);

static_assert(offsetof(acpMetaXml_t, longname_ja) == 0x038C);
static_assert(offsetof(acpMetaXml_t, shortname_ja) == 0x1B8C);
static_assert(offsetof(acpMetaXml_t, publisher_ja) == 0x278C);

static_assert(sizeof(acpMetaData_t) == 0x1AB00);
static_assert(offsetof(acpMetaData_t, bootMovie) == 0);
static_assert(offsetof(acpMetaData_t, bootLogoTex) == 0x13B38);

namespace iosu
{

	struct
	{
		bool isInitialized;
	}iosuAcp = { 0 };

	void _xml_parseU32(tinyxml2::XMLElement* xmlElement, const char* name, uint32be* v)
	{
		tinyxml2::XMLElement* subElement = xmlElement->FirstChildElement(name);
		*v = 0;
		if (subElement == nullptr)
			return;
		const char* text = subElement->GetText();
		uint32 value;
		if (sscanf(text, "%u", &value) == 0)
			return;
		*v = value;
	}

	void _xml_parseHex16(tinyxml2::XMLElement* xmlElement, const char* name, uint16be* v)
	{
		tinyxml2::XMLElement* subElement = xmlElement->FirstChildElement(name);
		*v = 0;
		if (subElement == nullptr)
			return;
		const char* text = subElement->GetText();
		uint32 value;
		if (sscanf(text, "%x", &value) == 0)
			return;
		*v = value;
	}

	void _xml_parseHex32(tinyxml2::XMLElement* xmlElement, const char* name, uint32be* v)
	{
		tinyxml2::XMLElement* subElement = xmlElement->FirstChildElement(name);
		*v = 0;
		if (subElement == nullptr)
			return;
		const char* text = subElement->GetText();
		uint32 value;
		if (sscanf(text, "%x", &value) == 0)
			return;
		*v = value;
	}

	void _xml_parseHex64(tinyxml2::XMLElement* xmlElement, const char* name, uint64* v)
	{
		tinyxml2::XMLElement* subElement = xmlElement->FirstChildElement(name);
		*v = 0;
		if (subElement == nullptr)
			return;
		const char* text = subElement->GetText();
		uint64 value;
		if (sscanf(text, "%" SCNx64, &value) == 0)
			return;
		*v = _swapEndianU64(value);
	}

	void _xml_parseString_(tinyxml2::XMLElement* xmlElement, const char* name, char* output, sint32 maxLength)
	{
		tinyxml2::XMLElement* subElement = xmlElement->FirstChildElement(name);
		output[0] = '\0';
		if (subElement == nullptr)
			return;
		const char* text = subElement->GetText();
		if (text == nullptr)
		{
			output[0] = '\0';
			return;
		}
		strncpy(output, text, maxLength - 1);
		output[maxLength - 1] = '\0';
	}

#define _metaXml_parseString(__xmlElement, __name, __output) _xml_parseString_(__xmlElement, __name, __output, sizeof(__output));

	void parseSaveMetaXml(uint8* metaXmlData, sint32 metaXmlLength, acpMetaXml_t* metaXml)
	{
		memset(metaXml, 0, sizeof(acpMetaXml_t));
		tinyxml2::XMLDocument appXml;
		appXml.Parse((const char*)metaXmlData, metaXmlLength);
		uint32 titleVersion = 0xFFFFFFFF;
		tinyxml2::XMLElement* menuElement = appXml.FirstChildElement("menu");
		if (menuElement)
		{
			_xml_parseHex64(menuElement, "title_id", &metaXml->title_id);
			_xml_parseHex64(menuElement, "boss_id", &metaXml->boss_id);
			_xml_parseHex64(menuElement, "os_version", &metaXml->os_version);
			_xml_parseHex64(menuElement, "app_size", &metaXml->app_size);
			_xml_parseHex64(menuElement, "common_save_size", &metaXml->common_save_size);
			_xml_parseHex64(menuElement, "account_save_size", &metaXml->account_save_size);
			_xml_parseHex64(menuElement, "common_boss_size", &metaXml->common_boss_size);
			_xml_parseHex64(menuElement, "account_boss_size", &metaXml->account_boss_size);
			_xml_parseHex64(menuElement, "join_game_mode_mask", &metaXml->join_game_mode_mask);
			_xml_parseU32(menuElement, "version", &metaXml->version);
			_metaXml_parseString(menuElement, "product_code", metaXml->product_code);
			_metaXml_parseString(menuElement, "content_platform", metaXml->content_platform);
			_metaXml_parseString(menuElement, "company_code", metaXml->company_code);
			_metaXml_parseString(menuElement, "mastering_date", metaXml->mastering_date);
			_xml_parseU32(menuElement, "logo_type", &metaXml->logo_type);

			_xml_parseU32(menuElement, "app_launch_type", &metaXml->app_launch_type);
			_xml_parseU32(menuElement, "invisible_flag", &metaXml->invisible_flag);
			_xml_parseU32(menuElement, "no_managed_flag", &metaXml->no_managed_flag);
			_xml_parseU32(menuElement, "no_event_log", &metaXml->no_event_log);
			_xml_parseU32(menuElement, "no_icon_database", &metaXml->no_icon_database);
			_xml_parseU32(menuElement, "launching_flag", &metaXml->launching_flag);
			_xml_parseU32(menuElement, "install_flag", &metaXml->install_flag);
			_xml_parseU32(menuElement, "closing_msg", &metaXml->closing_msg);
			_xml_parseU32(menuElement, "title_version", &metaXml->title_version);
			_xml_parseHex32(menuElement, "group_id", &metaXml->group_id);
			_xml_parseU32(menuElement, "save_no_rollback", &metaXml->save_no_rollback);
			_xml_parseU32(menuElement, "bg_daemon_enable", &metaXml->bg_daemon_enable);
			_xml_parseHex32(menuElement, "join_game_id", &metaXml->join_game_id);

			_xml_parseU32(menuElement, "olv_accesskey", &metaXml->olv_accesskey);
			_xml_parseU32(menuElement, "wood_tin", &metaXml->wood_tin);
			_xml_parseU32(menuElement, "e_manual", &metaXml->e_manual);
			_xml_parseU32(menuElement, "e_manual_version", &metaXml->e_manual_version);
			_xml_parseHex32(menuElement, "region", &metaXml->region);

			_xml_parseU32(menuElement, "pc_cero", &metaXml->pc_cero);
			_xml_parseU32(menuElement, "pc_esrb", &metaXml->pc_esrb);
			_xml_parseU32(menuElement, "pc_bbfc", &metaXml->pc_bbfc);
			_xml_parseU32(menuElement, "pc_usk", &metaXml->pc_usk);
			_xml_parseU32(menuElement, "pc_pegi_gen", &metaXml->pc_pegi_gen);
			_xml_parseU32(menuElement, "pc_pegi_fin", &metaXml->pc_pegi_fin);
			_xml_parseU32(menuElement, "pc_pegi_prt", &metaXml->pc_pegi_prt);
			_xml_parseU32(menuElement, "pc_pegi_bbfc", &metaXml->pc_pegi_bbfc);

			_xml_parseU32(menuElement, "pc_cob", &metaXml->pc_cob);
			_xml_parseU32(menuElement, "pc_grb", &metaXml->pc_grb);
			_xml_parseU32(menuElement, "pc_cgsrr", &metaXml->pc_cgsrr);
			_xml_parseU32(menuElement, "pc_oflc", &metaXml->pc_oflc);

			_xml_parseU32(menuElement, "pc_reserved0", &metaXml->pc_reserved0);
			_xml_parseU32(menuElement, "pc_reserved1", &metaXml->pc_reserved1);
			_xml_parseU32(menuElement, "pc_reserved2", &metaXml->pc_reserved2);
			_xml_parseU32(menuElement, "pc_reserved3", &metaXml->pc_reserved3);

			_xml_parseU32(menuElement, "ext_dev_nunchaku", &metaXml->ext_dev_nunchaku);
			_xml_parseU32(menuElement, "ext_dev_classic", &metaXml->ext_dev_classic);
			_xml_parseU32(menuElement, "ext_dev_urcc", &metaXml->ext_dev_urcc);
			_xml_parseU32(menuElement, "ext_dev_board", &metaXml->ext_dev_board);
			_xml_parseU32(menuElement, "ext_dev_usb_keyboard", &metaXml->ext_dev_usb_keyboard);
			_xml_parseU32(menuElement, "ext_dev_etc", &metaXml->ext_dev_etc);

			_metaXml_parseString(menuElement, "ext_dev_etc_name", metaXml->ext_dev_etc_name);

			_xml_parseU32(menuElement, "eula_version", &metaXml->eula_version);
			_xml_parseU32(menuElement, "drc_use", &metaXml->drc_use);
			_xml_parseU32(menuElement, "network_use", &metaXml->network_use);
			_xml_parseU32(menuElement, "online_account_use", &metaXml->online_account_use);
			_xml_parseU32(menuElement, "direct_boot", &metaXml->direct_boot);
			_xml_parseU32(menuElement, "reserved_flag0", &(metaXml->reserved_flag[0]));
			_xml_parseU32(menuElement, "reserved_flag1", &(metaXml->reserved_flag[1]));
			_xml_parseU32(menuElement, "reserved_flag2", &(metaXml->reserved_flag[2]));
			_xml_parseU32(menuElement, "reserved_flag3", &(metaXml->reserved_flag[3]));
			_xml_parseU32(menuElement, "reserved_flag4", &(metaXml->reserved_flag[4]));
			_xml_parseU32(menuElement, "reserved_flag5", &(metaXml->reserved_flag[5]));
			_xml_parseU32(menuElement, "reserved_flag6", &(metaXml->reserved_flag[6]));
			_xml_parseU32(menuElement, "reserved_flag7", &(metaXml->reserved_flag[7]));

			_metaXml_parseString(menuElement, "longname_ja", metaXml->longname_ja);
			_metaXml_parseString(menuElement, "longname_en", metaXml->longname_en);
			_metaXml_parseString(menuElement, "longname_fr", metaXml->longname_fr);
			_metaXml_parseString(menuElement, "longname_de", metaXml->longname_de);
			_metaXml_parseString(menuElement, "longname_it", metaXml->longname_it);
			_metaXml_parseString(menuElement, "longname_es", metaXml->longname_es);
			_metaXml_parseString(menuElement, "longname_zhs", metaXml->longname_zhs);
			_metaXml_parseString(menuElement, "longname_ko", metaXml->longname_ko);
			_metaXml_parseString(menuElement, "longname_nl", metaXml->longname_nl);
			_metaXml_parseString(menuElement, "longname_pt", metaXml->longname_pt);
			_metaXml_parseString(menuElement, "longname_ru", metaXml->longname_ru);
			_metaXml_parseString(menuElement, "longname_zht", metaXml->longname_zht);

			_metaXml_parseString(menuElement, "shortname_ja", metaXml->shortname_ja);
			_metaXml_parseString(menuElement, "shortname_en", metaXml->shortname_en);
			_metaXml_parseString(menuElement, "shortname_fr", metaXml->shortname_fr);
			_metaXml_parseString(menuElement, "shortname_de", metaXml->shortname_de);
			_metaXml_parseString(menuElement, "shortname_it", metaXml->shortname_it);
			_metaXml_parseString(menuElement, "shortname_es", metaXml->shortname_es);
			_metaXml_parseString(menuElement, "shortname_zhs", metaXml->shortname_zhs);
			_metaXml_parseString(menuElement, "shortname_ko", metaXml->shortname_ko);
			_metaXml_parseString(menuElement, "shortname_nl", metaXml->shortname_nl);
			_metaXml_parseString(menuElement, "shortname_pt", metaXml->shortname_pt);
			_metaXml_parseString(menuElement, "shortname_ru", metaXml->shortname_ru);
			_metaXml_parseString(menuElement, "shortname_zht", metaXml->shortname_zht);

			_metaXml_parseString(menuElement, "publisher_ja", metaXml->publisher_ja);
			_metaXml_parseString(menuElement, "publisher_en", metaXml->publisher_en);
			_metaXml_parseString(menuElement, "publisher_fr", metaXml->publisher_fr);
			_metaXml_parseString(menuElement, "publisher_de", metaXml->publisher_de);
			_metaXml_parseString(menuElement, "publisher_it", metaXml->publisher_it);
			_metaXml_parseString(menuElement, "publisher_es", metaXml->publisher_es);
			_metaXml_parseString(menuElement, "publisher_zhs", metaXml->publisher_zhs);
			_metaXml_parseString(menuElement, "publisher_ko", metaXml->publisher_ko);
			_metaXml_parseString(menuElement, "publisher_nl", metaXml->publisher_nl);
			_metaXml_parseString(menuElement, "publisher_pt", metaXml->publisher_pt);
			_metaXml_parseString(menuElement, "publisher_ru", metaXml->publisher_ru);
			_metaXml_parseString(menuElement, "publisher_zht", metaXml->publisher_zht);

			for (sint32 i = 0; i < 32; i++)
			{
				char tempStr[256];
				sprintf(tempStr, "add_on_unique_id%d", i);
				_xml_parseU32(menuElement, tempStr, &(metaXml->add_on_unique_id[i]));
			}
		}
	}

	bool _is8DigitHex(const char* str)
	{
		if (strlen(str) != 8)
			return false;
		for (sint32 f = 0; f < 8; f++)
		{
			if (str[f] >= '0' && str[f] <= '9')
				continue;
			if (str[f] >= 'a' && str[f] <= 'f')
				continue;
			if (str[f] >= 'A' && str[f] <= 'F')
				continue;
			return false;
		}
		return true;
	}

	sint32 ACPGetSaveDataTitleIdList(uint32 storageDeviceGuessed, acpTitleId_t* titleIdList, sint32 maxCount, uint32be* countOut)
	{
		sint32 count = 0;

		const char* devicePath = "/vol/storage_mlc01/";
		if (storageDeviceGuessed != 3)
			cemu_assert_unimplemented();
		char searchPath[FSA_CMD_PATH_MAX_LENGTH];
		sprintf(searchPath, "%susr/save/", devicePath);
		sint32 fscStatus = 0;
		FSCVirtualFile* fscDirIteratorTitleIdHigh = fsc_openDirIterator(searchPath, &fscStatus);
		FSCDirEntry dirEntryTitleIdHigh;
		FSCDirEntry dirEntryTitleIdLow;
		if(fscDirIteratorTitleIdHigh)
		{ 
			while (fsc_nextDir(fscDirIteratorTitleIdHigh, &dirEntryTitleIdHigh))
			{
				// is 8-digit hex?
				if(_is8DigitHex(dirEntryTitleIdHigh.path) == false)
					continue;
				uint32 titleIdHigh;
				sscanf(dirEntryTitleIdHigh.path, "%x", &titleIdHigh);
				sprintf(searchPath, "%susr/save/%08x/", devicePath, titleIdHigh);
				FSCVirtualFile* fscDirIteratorTitleIdLow = fsc_openDirIterator(searchPath, &fscStatus);
				if (fscDirIteratorTitleIdLow)
				{
					while (fsc_nextDir(fscDirIteratorTitleIdLow, &dirEntryTitleIdLow))
					{
						// is 8-digit hex?
						if (_is8DigitHex(dirEntryTitleIdLow.path) == false)
							continue;
						uint32 titleIdLow;
						sscanf(dirEntryTitleIdLow.path, "%x", &titleIdLow);
						// check if /meta/meta.xml exists
						char tempPath[FSA_CMD_PATH_MAX_LENGTH];
						sprintf(tempPath, "%susr/save/%08x/%08x/meta/meta.xml", devicePath, titleIdHigh, titleIdLow);
						if (fsc_doesFileExist(tempPath))
						{
							if (count < maxCount)
							{
								titleIdList[count].titleIdHigh = titleIdHigh;
								titleIdList[count].titleIdLow = titleIdLow;
								count++;
							}
						}
						else
						{
							cemuLog_logDebug(LogType::Force, "ACPGetSaveDataTitleIdList(): Missing meta.xml for save {:08x}-{:08x}", titleIdHigh, titleIdLow);
						}
					}
					fsc_close(fscDirIteratorTitleIdLow);
				}
			}
			fsc_close(fscDirIteratorTitleIdHigh);
		}
		*countOut = count;
		return 0;
	}

	sint32 ACPGetTitleSaveMetaXml(uint64 titleId, acpMetaXml_t* acpMetaXml, sint32 uknType)
	{
		// uknType is probably the storage device?
		if (uknType != 3) // mlc01 ?
			assert_dbg();

		char xmlPath[FSA_CMD_PATH_MAX_LENGTH];
		sprintf(xmlPath, "%susr/save/%08x/%08x/meta/meta.xml", "/vol/storage_mlc01/", (uint32)(titleId>>32), (uint32)(titleId&0xFFFFFFFF));

		uint32 saveMetaXmlSize = 0;
		uint8* saveMetaXmlData = fsc_extractFile(xmlPath, &saveMetaXmlSize);
		if (saveMetaXmlData)
		{
			parseSaveMetaXml(saveMetaXmlData, saveMetaXmlSize, acpMetaXml);
			free(saveMetaXmlData);
		}
		else
		{
			cemuLog_log(LogType::Force, "ACPGetTitleSaveMetaXml(): Meta file \"{}\" does not exist", xmlPath);
			memset(acpMetaXml, 0, sizeof(acpMetaXml_t));
		}
		return 0;
	}

	sint32 ACPGetTitleMetaData(uint64 titleId, acpMetaData_t* acpMetaData)
	{
		memset(acpMetaData, 0, sizeof(acpMetaData_t));

		char titlePath[1024];

		if (((titleId >> 32) & 0x10) != 0)
		{
			sprintf(titlePath, "/vol/storage_mlc01/sys/title/%08x/%08x/", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		}
		else
		{
			sprintf(titlePath, "/vol/storage_mlc01/usr/title/%08x/%08x/", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		}


		char filePath[FSA_CMD_PATH_MAX_LENGTH];
		sprintf(filePath, "%smeta/bootMovie.h264", titlePath);

		// bootMovie.h264
		uint32 metaBootMovieSize = 0;
		uint8* metaBootMovieData = fsc_extractFile(filePath, &metaBootMovieSize);
		if (metaBootMovieData)
		{
			memcpy(acpMetaData->bootMovie, metaBootMovieData, std::min<uint32>(metaBootMovieSize, sizeof(acpMetaData->bootMovie)));
			free(metaBootMovieData);
		}
		else
			cemuLog_log(LogType::Force, "ACPGetTitleMetaData(): Unable to load \"{}\"", filePath);
		// bootLogoTex.tga
		sprintf(filePath, "%smeta/bootLogoTex.tga", titlePath);
		uint32 metaBootLogoSize = 0;
		uint8* metaBootLogoData = fsc_extractFile(filePath, &metaBootLogoSize);
		if (metaBootLogoData)
		{
			memcpy(acpMetaData->bootLogoTex, metaBootLogoData, std::min<uint32>(metaBootLogoSize, sizeof(acpMetaData->bootLogoTex)));
			free(metaBootLogoData);
		}
		else
			cemuLog_log(LogType::Force, "ACPGetTitleMetaData(): Unable to load \"{}\"", filePath);


		return 0;
	}

	sint32 ACPGetTitleMetaXml(uint64 titleId, acpMetaXml_t* acpMetaXml)
	{
		memset(acpMetaXml, 0, sizeof(acpMetaXml_t));

		char titlePath[1024];

		if (((titleId >> 32) & 0x10) != 0)
		{
			sprintf(titlePath, "/vol/storage_mlc01/sys/title/%08x/%08x/", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		}
		else
		{
			sprintf(titlePath, "/vol/storage_mlc01/usr/title/%08x/%08x/", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		}


		char filePath[FSA_CMD_PATH_MAX_LENGTH];
		sprintf(filePath, "%smeta/meta.xml", titlePath);

		uint32 metaXmlSize = 0;
		uint8* metaXmlData = fsc_extractFile(filePath, &metaXmlSize);
		if (metaXmlData)
		{
			parseSaveMetaXml(metaXmlData, metaXmlSize, acpMetaXml);
			free(metaXmlData);
		}
		else
		{
			cemuLog_log(LogType::Force, "ACPGetTitleMetaXml(): Meta file \"{}\" does not exist", filePath);
		}

		return 0;
	}

	sint32 ACPGetTitleSaveDirEx(uint64 titleId, uint32 storageDeviceGuessed, acpSaveDirInfo_t* saveDirInfo, sint32 maxCount, uint32be* countOut)
	{
		sint32 count = 0;

		const char* devicePath = "/vol/storage_mlc01/";
		if (storageDeviceGuessed != 3)
			cemu_assert_unimplemented();

		char searchPath[FSA_CMD_PATH_MAX_LENGTH];
		char tempPath[FSA_CMD_PATH_MAX_LENGTH];

		sint32 fscStatus = 0;
		// add common dir
		sprintf(searchPath, "%susr/save/%08x/%08x/user/common/", devicePath, (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		if (fsc_doesDirectoryExist(searchPath))
		{
			acpSaveDirInfo_t* entry = saveDirInfo + count;
			if (count < maxCount)
			{
				// get dir size
				sprintf(tempPath, "%susr/save/%08x/%08x/user/common/", devicePath, (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
				FSCVirtualFile* fscDir = fsc_open(tempPath, FSC_ACCESS_FLAG::OPEN_DIR, &fscStatus);
				uint64 dirSize = 0;
				if (fscDir)
				{
					dirSize = fsc_getFileSize(fscDir);
					fsc_close(fscDir);
				}

				memset(entry, 0, sizeof(acpSaveDirInfo_t));
				entry->ukn00 = (uint32)(titleId>>32);
				entry->ukn04 = (uint32)(titleId&0xFFFFFFFF);
				entry->persistentId = 0; // 0 -> common save
				entry->ukn0C = 0;
				entry->sizeA = _swapEndianU64(0); // ukn
				entry->sizeB = _swapEndianU64(dirSize);
				entry->time = _swapEndianU64((coreinit::coreinit_getOSTime() / ESPRESSO_TIMER_CLOCK));
				sprintf(entry->path, "%susr/save/%08x/%08x/meta/", devicePath, (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
				count++;
			}
		}
		// add user directories
		sprintf(searchPath, "%susr/save/%08x/%08x/user/", devicePath, (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
		FSCVirtualFile* fscDirIterator = fsc_openDirIterator(searchPath, &fscStatus);
		if (fscDirIterator == nullptr)
		{
			cemuLog_log(LogType::Force, "ACPGetTitleSaveDirEx(): Failed to iterate directories in \"{}\"", searchPath);
			*countOut = 0;
		}
		else
		{
			FSCDirEntry dirEntry;
			while( fsc_nextDir(fscDirIterator, &dirEntry) )
			{
				if(dirEntry.isDirectory == false)
					continue;
				// is 8-digit hex name? (persistent id)
				if(_is8DigitHex(dirEntry.path) == false )
					continue;
				uint32 persistentId = 0;
				sscanf(dirEntry.path, "%x", &persistentId);
				acpSaveDirInfo_t* entry = saveDirInfo + count;
				if (count < maxCount)
				{
					memset(entry, 0, sizeof(acpSaveDirInfo_t));
					entry->ukn00 = (uint32)(titleId >> 32); // titleId?
					entry->ukn04 = (uint32)(titleId & 0xFFFFFFFF); // titleId?
					entry->persistentId = persistentId; // 0 -> common save
					entry->ukn0C = 0;
					entry->sizeA = _swapEndianU64(0);
					entry->sizeB = _swapEndianU64(0);
					entry->time = _swapEndianU64((coreinit::coreinit_getOSTime() / ESPRESSO_TIMER_CLOCK));
					sprintf(entry->path, "%susr/save/%08x/%08x/meta/", devicePath, (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF));
					count++;
				}
			}
			fsc_close(fscDirIterator);
		}
		*countOut = count;
		return 0;
	}

	int iosuAcp_thread()
	{
		SetThreadName("iosuAcp_thread");
		while (true)
		{
			uint32 returnValue = 0; // Ioctl return value
			ioQueueEntry_t* ioQueueEntry = iosuIoctl_getNextWithWait(IOS_DEVICE_ACP_MAIN);
			if (ioQueueEntry->request == IOSU_ACP_REQUEST_CEMU)
			{
				iosuAcpCemuRequest_t* acpCemuRequest = (iosuAcpCemuRequest_t*)ioQueueEntry->bufferVectors[0].buffer.GetPtr();
				if (acpCemuRequest->requestCode == IOSU_ACP_GET_SAVE_DATA_TITLE_ID_LIST)
				{
					uint32be count = 0;
					acpCemuRequest->returnCode = ACPGetSaveDataTitleIdList(acpCemuRequest->type, (acpTitleId_t*)acpCemuRequest->ptr.GetPtr(), acpCemuRequest->maxCount, &count);
					acpCemuRequest->resultU32.u32 = count;
				}
				else if (acpCemuRequest->requestCode == IOSU_ACP_GET_TITLE_SAVE_META_XML)
				{
					acpCemuRequest->returnCode = ACPGetTitleSaveMetaXml(acpCemuRequest->titleId, (acpMetaXml_t*)acpCemuRequest->ptr.GetPtr(), acpCemuRequest->type);
				}
				else if (acpCemuRequest->requestCode == IOSU_ACP_GET_TITLE_SAVE_DIR)
				{
					uint32be count = 0;
					acpCemuRequest->returnCode = ACPGetTitleSaveDirEx(acpCemuRequest->titleId, acpCemuRequest->type, (acpSaveDirInfo_t*)acpCemuRequest->ptr.GetPtr(), acpCemuRequest->maxCount, &count);
					acpCemuRequest->resultU32.u32 = count;
				}
				else if (acpCemuRequest->requestCode == IOSU_ACP_GET_TITLE_META_DATA)
				{
					acpCemuRequest->returnCode = ACPGetTitleMetaData(acpCemuRequest->titleId, (acpMetaData_t*)acpCemuRequest->ptr.GetPtr());
				}
				else if (acpCemuRequest->requestCode == IOSU_ACP_GET_TITLE_META_XML)
				{
					acpCemuRequest->returnCode = ACPGetTitleMetaXml(acpCemuRequest->titleId, (acpMetaXml_t*)acpCemuRequest->ptr.GetPtr());
				}
				else if (acpCemuRequest->requestCode == IOSU_ACP_CREATE_SAVE_DIR_EX)
				{
					acpCemuRequest->returnCode = acp::ACPCreateSaveDirEx(acpCemuRequest->accountSlot, acpCemuRequest->titleId);
				}
				else
					cemu_assert_unimplemented();
			}
			else
				cemu_assert_unimplemented();
			iosuIoctl_completeRequest(ioQueueEntry, returnValue);
		}
		return 0;
	}

	void iosuAcp_init()
	{
		if (iosuAcp.isInitialized)
			return;
		std::thread t(iosuAcp_thread);
		t.detach();
		iosuAcp.isInitialized = true;
	}

	bool iosuAcp_isInitialized()
	{
		return iosuAcp.isInitialized;
	}

	/* Above is the legacy implementation. Below is the new style implementation which also matches the official IPC protocol and works with the real nn_acp.rpl */

	namespace acp
	{

		uint64 _ACPGetTimestamp()
		{
			return coreinit::coreinit_getOSTime() / ESPRESSO_TIMER_CLOCK;
		}

		nnResult ACPUpdateSaveTimeStamp(uint32 persistentId, uint64 titleId, ACPDeviceType deviceType)
		{
			if (deviceType == ACPDeviceType::UnknownType)
			{
				return (nnResult)0xA030FB80;
			}

			// create or modify the saveinfo
			const auto saveinfoPath = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/meta/saveinfo.xml", GetTitleIdHigh(titleId), GetTitleIdLow(titleId));
			auto saveinfoData = FileStream::LoadIntoMemory(saveinfoPath);
			if (saveinfoData && !saveinfoData->empty())
			{
				namespace xml = tinyxml2;
				xml::XMLDocument doc;
				tinyxml2::XMLError xmlError = doc.Parse((const char*)saveinfoData->data(), saveinfoData->size());
				if (xmlError == xml::XML_SUCCESS || xmlError == xml::XML_ERROR_EMPTY_DOCUMENT)
				{
					xml::XMLNode* child = doc.FirstChild();
					// check for declaration -> <?xml version="1.0" encoding="utf-8"?>
					if (!child || !child->ToDeclaration())
					{
						xml::XMLDeclaration* decl = doc.NewDeclaration();
						doc.InsertFirstChild(decl);
					}

					xml::XMLElement* info = doc.FirstChildElement("info");
					if (!info)
					{
						info = doc.NewElement("info");
						doc.InsertEndChild(info);
					}

					// find node with persistentId
					char tmp[64];
					sprintf(tmp, "%08x", persistentId);
					bool foundNode = false;
					for (xml::XMLElement* account = info->FirstChildElement("account"); account; account = account->NextSiblingElement("account"))
					{
						if (account->Attribute("persistentId", tmp))
						{
							// found the entry! -> update timestamp
							xml::XMLElement* timestamp = account->FirstChildElement("timestamp");
							sprintf(tmp, "%" PRIx64, _ACPGetTimestamp());
							if (timestamp)
								timestamp->SetText(tmp);
							else
							{
								timestamp = doc.NewElement("timestamp");
								account->InsertFirstChild(timestamp);
							}

							foundNode = true;
							break;
						}
					}

					if (!foundNode)
					{
						tinyxml2::XMLElement* account = doc.NewElement("account");
						{
							sprintf(tmp, "%08x", persistentId);
							account->SetAttribute("persistentId", tmp);

							tinyxml2::XMLElement* timestamp = doc.NewElement("timestamp");
							{
								sprintf(tmp, "%" PRIx64, _ACPGetTimestamp());
								timestamp->SetText(tmp);
							}

							account->InsertFirstChild(timestamp);
						}

						info->InsertFirstChild(account);
					}

					// update file
					tinyxml2::XMLPrinter printer;
					doc.Print(&printer);
					FileStream* fs = FileStream::createFile2(saveinfoPath);
					if (fs)
					{
						fs->writeString(printer.CStr());
						delete fs;
					}
				}
			}
			return NN_RESULT_SUCCESS;
		}

		void CreateSaveMetaFiles(uint32 persistentId, uint64 titleId)
		{
			std::string titlePath = CafeSystem::GetMlcStoragePath(CafeSystem::GetForegroundTitleId());

			sint32 fscStatus;
			FSCVirtualFile* fscFile = fsc_open((titlePath + "/meta/meta.xml").c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
			if (fscFile)
			{
				sint32 fileSize = fsc_getFileSize(fscFile);

				std::unique_ptr<uint8[]> fileContent = std::make_unique<uint8[]>(fileSize);
				fsc_readFile(fscFile, fileContent.get(), fileSize);
				fsc_close(fscFile);

				const auto outPath = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/meta/meta.xml", GetTitleIdHigh(titleId), GetTitleIdLow(titleId));

				std::ofstream myFile(outPath, std::ios::out | std::ios::binary);
				myFile.write((char*)fileContent.get(), fileSize);
				myFile.close();
			}

			fscFile = fsc_open((titlePath + "/meta/iconTex.tga").c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
			if (fscFile)
			{
				sint32 fileSize = fsc_getFileSize(fscFile);

				std::unique_ptr<uint8[]> fileContent = std::make_unique<uint8[]>(fileSize);
				fsc_readFile(fscFile, fileContent.get(), fileSize);
				fsc_close(fscFile);

				const auto outPath = ActiveSettings::GetMlcPath("usr/save/{:08x}/{:08x}/meta/iconTex.tga", GetTitleIdHigh(titleId), GetTitleIdLow(titleId));

				std::ofstream myFile(outPath, std::ios::out | std::ios::binary);
				myFile.write((char*)fileContent.get(), fileSize);
				myFile.close();
			}

			ACPUpdateSaveTimeStamp(persistentId, titleId, iosu::acp::ACPDeviceType::InternalDeviceType);
		}


		sint32 _ACPCreateSaveDir(uint32 persistentId, uint64 titleId, ACPDeviceType type)
		{
			uint32 high = GetTitleIdHigh(titleId) & (~0xC);
			uint32 low = GetTitleIdLow(titleId);

			sint32 fscStatus = FSC_STATUS_FILE_NOT_FOUND;
			char path[256];

			sprintf(path, "%susr/boss/", "/vol/storage_mlc01/");
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/boss/%08x/", "/vol/storage_mlc01/", high);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/boss/%08x/%08x/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/boss/%08x/%08x/user/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/boss/%08x/%08x/user/common", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/boss/%08x/%08x/user/%08x/", "/vol/storage_mlc01/", high, low, persistentId == 0 ? 0x80000001 : persistentId);
			fsc_createDir(path, &fscStatus);

			sprintf(path, "%susr/save/%08x/", "/vol/storage_mlc01/", high);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/meta/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/user/", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/user/common", "/vol/storage_mlc01/", high, low);
			fsc_createDir(path, &fscStatus);
			sprintf(path, "%susr/save/%08x/%08x/user/%08x", "/vol/storage_mlc01/", high, low, persistentId == 0 ? 0x80000001 : persistentId);
			fsc_createDir(path, &fscStatus);

			// copy xml meta files
			CreateSaveMetaFiles(persistentId, titleId);
			return 0;
		}

		nnResult ACPCreateSaveDir(uint32 persistentId, ACPDeviceType type)
		{
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			return _ACPCreateSaveDir(persistentId, titleId, type);
		}

		sint32 ACPCreateSaveDirEx(uint8 accountSlot, uint64 titleId)
		{
			uint32 persistentId = 0;
			cemu_assert_debug(accountSlot >= 1 && accountSlot <= 13); // outside valid slot range?
			bool r = iosu::act::GetPersistentId(accountSlot, &persistentId);
			cemu_assert_debug(r);
			return _ACPCreateSaveDir(persistentId, titleId, ACPDeviceType::InternalDeviceType);
		}

		nnResult ACPGetOlvAccesskey(uint32be* accessKey)
		{
			*accessKey = CafeSystem::GetForegroundTitleOlvAccesskey();
			return 0;
		}

		class AcpMainService : public iosu::nn::IPCService
		{
		  public:
			AcpMainService() : iosu::nn::IPCService("/dev/acp_main") {}

			nnResult ServiceCall(uint32 serviceId, void* request, void* response) override
			{
				cemuLog_log(LogType::Force, "Unsupported service call to /dev/acp_main");
				cemu_assert_unimplemented();
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_ACP, 0);
			}
		};

		AcpMainService gACPMainService;

		class : public ::IOSUModule
		{
			void TitleStart() override
			{
				gACPMainService.Start();
				// gACPMainService.SetTimerUpdate(1000); // call TimerUpdate() once a second
			}
			void TitleStop() override
			{
				gACPMainService.Stop();
			}
		}sIOSUModuleNNACP;

		IOSUModule* GetModule()
		{
			return static_cast<IOSUModule*>(&sIOSUModuleNNACP);
		}
	} // namespace acp
} // namespace iosu
