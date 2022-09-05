#include "iosu_boss.h"

#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <array>
#include <queue>
#include <filesystem>
#include <fstream>

#include "config/ActiveSettings.h"
#include "curl/curl.h"
#include "openssl/bn.h"
#include "openssl/x509.h"
#include "openssl/ssl.h"

#include "iosu_ioctl.h"
#include "Cafe/OS/libs/nn_common.h"
#include "iosu_act.h"
#include "iosu_crypto.h"
#include "util/crypto/aes128.h"
#include "config/CemuConfig.h"

#include "util/helpers/helpers.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "Cafe/CafeSystem.h"

namespace iosu
{
	enum TurnState
	{
		kUnknown = 0,
		kStopped = 1,
		kStoppedByPolicyList = 2,
		kWaitTime = 3,
		kWaitRun = 4,
		kWaitResume = 5,
		kRunning = 6,
		kFinished = 7,
		kSuccess = 16,
		kError = 17,
	};

	enum class ContentType
	{
		kUnknownContent,
		kXmlContent,
		kBinaryFile,
		kText,
	};

	enum class FileType
	{
		kUnknownFile,
		kAppData,
	};

	enum TaskSettingType : uint32
	{
		kRawDlTaskSetting = 0x10000698,
	};

	struct TaskFile
	{
		TaskFile(std::string file_name, uint32 data_id, FileType type, std::string url, uint32 size)
			: file_name(file_name), data_id(data_id), file_type(type), url(url), size(size) {}

		std::string file_name;
		uint32 data_id;
		FileType file_type;
		std::string url;
		uint32 size;
		uint64 last_modified = 0;
	};

	struct TaskSetting
	{
		static const uint32 kBossCode = 0x7C0;
		static const uint32 kBossCodeLen = 0x20;

		static const uint32 kURL = 0x48;
		static const uint32 kURLLen = 0x100;

		static const uint32 kClientCert = 0x41;
		static const uint32 kCACert = 0x188;

		static const uint32 kServiceToken = 0x590;
		static const uint32 kServiceTokenLen = 0x200;

		static const uint32 kDirectorySizeLimit = 0x7F0;
		static const uint32 kDirectoryName = 0x7C8;
		static const uint32 kDirectoryNameLen = 0x8;

		static const uint32 kFileName = 0x7D0;
		static const uint32 kNbdlFileName = 0x7F8;
		static const uint32 kFileNameLen = 0x20;

		std::array<uint8, 0x1000> settings;
		uint32be taskType; // +0x1000
	};

	static_assert(sizeof(TaskSetting) == 0x1004, "sizeof(TaskSetting_t)");

	struct Task
	{
		char task_id[8]{};
		uint32 account_id;
		uint64 title_id;

		TaskSetting task_settings;

		uint32 exec_count = 0;
		std::shared_ptr<CURL> curl;
		uint64 content_length = 0;
		uint64 processed_length = 0;

		uint32 turn_state = 0;
		uint32 wait_state = 0;

		uint32 http_status_code = 0;
		ContentType content_type = ContentType::kUnknownContent;

		std::vector<uint8> result_buffer;

		std::queue<TaskFile> queued_files;
		std::vector<uint8> file_buffer;
		uint32 processed_file_size = 0;

		Task(const char* id, uint32 account_id, uint64 title_id, TaskSetting* settings)
		{
			strncpy(task_id, id, sizeof(task_id));
			this->account_id = account_id;
			this->title_id = title_id;
			this->task_settings.settings = settings->settings;
			this->task_settings.taskType = settings->taskType;

			curl = std::shared_ptr<CURL>(curl_easy_init(), curl_easy_cleanup);
		}
	};

#define FAD_ENTRY_MAX_COUNT (512)

	struct BossStorageFadEntry
	{
		char name[0x20];
		uint32be fileNameId;
		uint32 ukn24;
		uint32 ukn28;
		uint32 ukn2C;
		uint32 ukn30;
		uint32be timestampRelated; // guessed
	};

	static_assert(sizeof(BossStorageFadEntry) == 0x38, "sizeof(BossStorageFadEntry)");

	struct BossStorageFadFile
	{
		uint8 _00[0x08];
		BossStorageFadEntry entries[FAD_ENTRY_MAX_COUNT];
	};

	static_assert(sizeof(BossStorageFadFile) == 28680, "sizeof(BossStorageFadFile)");

	struct BossNbdlHeader
	{
		/* +0x00 */
		uint32be magic;
		/* +0x04 */
		uint32be version; // guessed
		/* +0x08 */
		uint16be ukn08; // must always be 1
		/* +0x0A */
		uint16be ukn0A; // must always be 2
		/* +0x0C */
		uint8 nonce[0xC];
		/* +0x18 */
		uint32 padding18; // unused
		/* +0x1C */
		uint32 padding1C; // unused
		/* +0x20 */
		struct
		{
			uint8 uknHashData[0x20];
		} encryptedHeader;
	} ;

	static_assert(sizeof(BossNbdlHeader) == 0x40, "BossNbdlHeader has invalid size");
	static_assert(offsetof(BossNbdlHeader, encryptedHeader) == 0x20, "offsetof(BossNbdlHeader, encryptedHeader)");


	struct
	{
		bool is_initialized;

		std::vector<Task> tasks;
	} g_boss = {};

	/*
		X-BOSS-Closed
		X-BOSS-TitleId
		X-Boss-UniqueId
		�\r���\r�(X-BOSS-Digest
	
		LOAD:E01038B4 0000000C C %susr/boss/
		LOAD:E0103A6C 00000011 C %susr/boss/%08x/
		LOAD:E0103B04 00000016 C %susr/boss/%08x/%08x/
		LOAD:E0103B1C 0000001B C %susr/boss/%08x/%08x/user/
		LOAD:E0103B5C 00000020 C %susr/boss/%08x/%08x/user/%08x/
		LOAD:E0103B38 00000022 C %susr/boss/%08x/%08x/user/common/
		LOAD:E0103AC4 00000020 C %susr/boss/%08x/%08x/user/temp/
	
		LOAD:E01106DC 0000000A C /dev/boss
	
		LOAD:05063698 0000001C C /vol/storage_mlc01/usr/boss
		LOAD:E2299CA8 00000028 C /vol/storage_mlc01/usr/save/system/boss
	 */

	template <typename ... TArgs>
	curl_slist* append_header_param(struct curl_slist* list, const char* format, TArgs&& ... args)
	{
		return curl_slist_append(list, fmt::format(fmt::runtime(format), std::forward<TArgs>(args)...).c_str());
	}

	bool starts_with(const char* str, const char* pre)
	{
		const size_t len_string = strlen(str);
		const size_t len_prefix = strlen(pre);
		return len_string < len_prefix ? false : strncmp(pre, str, len_prefix) == 0;
	}

	size_t task_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
	{
		Task* task = (Task*)userdata;
		const size_t writeByteSize = size * nmemb;

		//if (task->result_buffer.size() - task->processed_length < writeByteSize)
		//	task->result_buffer.resize(task->result_buffer.size() + writeByteSize);
		//writeByteSize = min(writeByteSize, task->result_buffer.capacity() - task->processed_length);

		//forceLogDebug_printf("task_write_callback: %d (processed: %d)", writeByteSize, task->processed_length);
		if (writeByteSize > 0)
		{
			//memcpy(task->result_buffer.data() + task->processed_length, ptr, writeByteSize);
			task->result_buffer.insert(task->result_buffer.end(), ptr, ptr + writeByteSize);
			task->processed_length += writeByteSize;
		}

		return writeByteSize;
	}

	size_t task_download_header_callback(char* ptr, size_t size, size_t nitems, void* userdata)
	{
		//forceLogDebug_printf("\tHeader: %s", ptr);

		return size * nitems;
	}

	size_t task_download_filecallback(char* ptr, size_t size, size_t nmemb, void* userdata)
	{
		Task* task = (Task*)userdata;
		const size_t writeByteSize = size * nmemb;
		//writeByteSize = min(writeByteSize, task->file_buffer.capacity() - task->processed_file_size);
		if (writeByteSize > 0)
		{
			//memcpy(task->file_buffer.data() + task->processed_file_size, ptr, writeByteSize);
			task->file_buffer.insert(task->file_buffer.end(), ptr, ptr + writeByteSize);
			task->processed_file_size += writeByteSize;
		}
		return writeByteSize;
	}

	size_t task_header_callback(char* ptr, size_t size, size_t nitems, void* userdata)
	{
		Task* task = (Task*)userdata;
		if (starts_with(ptr, "Content-Length: "))
		{
			task->content_length = strtol(&ptr[16], nullptr, 0);
			task->result_buffer.clear();
			task->result_buffer.reserve(task->content_length);
			task->processed_length = 0;
		}
		else if (starts_with(ptr, "Content-Type: "))
		{
			const char* type = &ptr[14];
			if (starts_with(type, "application/xml"))
				task->content_type = ContentType::kXmlContent;
			else if (starts_with(type, "x-application/octet-stream"))
				task->content_type = ContentType::kBinaryFile;
			else if (starts_with(type, "text/html"))
				task->content_type = ContentType::kText;
			else
			{
				forceLogDebug_printf("task_header_callback: unknown content type > %s", type);
			}
		}
		else if (starts_with(ptr, "Last-Modified: "))
		{
			// TODO timestamp (?)
		}

		//forceLogDebug_printf("task_header_callback: len %d (%d) and type %d", task->content_length, task->result_buffer.capacity(), task->content_type);
		//forceLogDebug_printf("\t%s", ptr);
		return size * nitems;
	}

	static CURLcode task_sslctx_function(CURL* curl, void* sslctx, void* param)
	{
		auto task_settings = (TaskSetting*)param;
		if (task_settings->taskType == kRawDlTaskSetting)
		{
			forceLogDebug_printf("sslctx_function: adding client cert: %d", (int)task_settings->settings[TaskSetting::kClientCert]);
			if (!iosuCrypto_addClientCertificate(sslctx, task_settings->settings[TaskSetting::kClientCert]))
				assert_dbg();

			uint32 location = TaskSetting::kCACert;
			for (int i = 0; i < 3; ++i)
			{
				if (task_settings->settings[location] != 0)
				{
					forceLogDebug_printf("sslctx_function: adding ca cert: %d", (int)task_settings->settings[location]);
					if (!iosuCrypto_addCACertificate(sslctx, task_settings->settings[location]))
					{
						forceLog_printf("Failed to load CA certificate file");
						assert_dbg();
					}
				}

				location += TaskSetting::kCACert;
			}
		}
		else
		{
			if (!iosuCrypto_addCACertificate(sslctx, 105))
			{
				forceLog_printf("Failed to load certificate file");
				assert_dbg();
			}

			if (!iosuCrypto_addClientCertificate(sslctx, 3))
			{
				forceLog_printf("Failed to load client certificate file");
				assert_dbg();
			}
		}

		SSL_CTX_set_cipher_list((SSL_CTX*)sslctx, "AES256-SHA");
		// TLS_RSA_WITH_AES_256_CBC_SHA (in CURL it's called rsa_aes_256_sha)
		SSL_CTX_set_mode((SSL_CTX*)sslctx, SSL_MODE_AUTO_RETRY);
		SSL_CTX_set_verify_depth((SSL_CTX*)sslctx, 2);
		SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_PEER, nullptr);
		return CURLE_OK;
	}

	auto get_task(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = std::find_if(g_boss.tasks.begin(), g_boss.tasks.end(), [taskId, accountId, titleId](const Task& task)
		{
			return 0 == strncmp(taskId, task.task_id, sizeof(Task::task_id)) && accountId == task.account_id && titleId == task.
				title_id;
		});

		return it;
	}

	bool parse_xml_content(Task& task)
	{
		tinyxml2::XMLDocument doc;
		//cafeLog_writeLineToLog((char*)task.result_buffer.data());
		if (doc.Parse((const char*)task.result_buffer.data(), task.processed_length) != tinyxml2::XML_SUCCESS)
			return false;

		for (tinyxml2::XMLElement* sheet = doc.FirstChildElement("TaskSheet"); sheet; sheet = sheet->
		     NextSiblingElement("TaskSheet"))
		{
			const auto files = sheet->FirstChildElement("Files");
			if (!files)
				continue;

			for (tinyxml2::XMLElement* file = files->FirstChildElement("File"); file; file = file->NextSiblingElement("File"))
			{
				auto file_name = file->FirstChildElement("Filename");
				if (!file_name)
					continue;

				auto data_id = file->FirstChildElement("DataId");
				if (!data_id)
					continue;

				auto type = file->FirstChildElement("Type");
				if (!type)
					continue;

				auto url = file->FirstChildElement("Url");
				if (!url)
					continue;

				auto size = file->FirstChildElement("Size");
				if (!size)
					continue;

				FileType file_type;
				if (0 == strcmp(type->GetText(), "AppData"))
					file_type = FileType::kAppData;
				else
				{
					file_type = FileType::kUnknownFile;
				}

				task.queued_files.emplace(file_name->GetText(), data_id->IntText(), file_type, url->GetText(), size->IntText());
			}
		}

		return true;
	}

	const uint64 kTimeStampConvertSeconds = 946684800ULL;
	BossStorageFadEntry* boss_storage_fad_find_entry(BossStorageFadFile& fad_file, uint32 data_id)
	{
		for (auto& entry : fad_file.entries)
		{
			if (entry.fileNameId == data_id)
				return &entry;
		}

		return nullptr;
	}

	void boss_storage_fad_append_or_update(BossStorageFadFile& fad_file, const char* name, uint32 data_id, uint64 timestamp)
	{
		for (auto& entry : fad_file.entries)
		{
			if (entry.fileNameId == 0 || strcmp(entry.name, name) == 0)
			{
				entry.fileNameId = data_id;
				strcpy(entry.name, name);
				entry.timestampRelated = (uint32)(timestamp - kTimeStampConvertSeconds); // time since 2000
				return;
			}
		}
	}

	uint32 task_run(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = get_task(taskId, accountId, titleId);
		if (it == g_boss.tasks.cend())
		{
			//it->turn_state = kError;
			//it->wait_state = TRUE;
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_BOSS, 0);
		}

		if (!ActiveSettings::IsOnlineEnabled())
		{
			it->turn_state = kError;
			it->wait_state = TRUE;
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
		}
		forceLogDebug_printf("task run state: %d | exec: %d (tasks: %d)", it->turn_state, it->exec_count, g_boss.tasks.size());
		it->turn_state = kRunning;
		it->exec_count++;

		/*
	https://nppl.app.nintendo.net/p01/policylist/1/1/AT
	https://npts.app.nintendo.net/p01/tasksheet/1/zvGSM4kO***kKnpT/schdat2?c=XX&l=en
	https://npts.app.nintendo.net/p01/tasksheet/1/zvGSM4kO***kKnpT/optdat2?c=XX&l=en
	https://npts.app.nintendo.net/p01/tasksheet/1/8UsM86l***jFk8z/wood1?c=XX&l=en
	https://npts.app.nintendo.net/p01/tasksheet/1/8UsM86l***kjFk8z/woodBGM?c=XX&l=en
	
	
	https://npts.app.nintendo.net/p01/tasksheet/%s/%s/%s/%s?c=%s&l=%s
	https://npts.app.nintendo.net/p01/tasksheet/%s/%s/%s?c=%s&l=%s
		1 == version
		bossCode
		initFile
	
		 */

		uint32 turnstate = kSuccess;
		struct curl_slist* list_headerParam = nullptr;
		list_headerParam = append_header_param(list_headerParam, "X-BOSS-Digest"); // ??? 
		list_headerParam = append_header_param(list_headerParam, "X-Boss-UniqueId: {:05x}", ((titleId >> 8) & 0xFFFFF)); // %05x
		list_headerParam = append_header_param(list_headerParam, "X-BOSS-TitleId: /usr/packages/title/{:016x}", titleId); // "/usr/packages/title/%016llx"
		
		CURL* curl = it->curl.get();
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 2);
#ifndef PUBLIC_RELEASE
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		char errbuf[CURL_ERROR_SIZE]{};
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
#else
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
#endif
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, task_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &(*it));
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, task_header_callback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &(*it));
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0x3C);
		curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, task_sslctx_function);
		curl_easy_setopt(curl, CURLOPT_SSL_CTX_DATA, &it->task_settings);
		curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_0);

		char url[512];
		if(it->task_settings.taskType == kRawDlTaskSetting)
		{
			char serviceToken[TaskSetting::kServiceTokenLen];
			strncpy(serviceToken, (char*)&it->task_settings.settings[TaskSetting::kServiceToken], TaskSetting::kServiceTokenLen);
			list_headerParam = append_header_param(list_headerParam, "X-Nintendo-ServiceToken: {}", serviceToken);

			strncpy(url, (char*)&it->task_settings.settings[TaskSetting::kURL], TaskSetting::kURLLen);
			forceLogDebug_printf("\tserviceToken: %s", serviceToken);
		}
		else
		{
			char languageCode[8];
			switch (GetConfig().console_language)
			{
			case CafeConsoleLanguage::JA:
				strcpy(languageCode, "ja");
				break;
			case CafeConsoleLanguage::EN:
				strcpy(languageCode, "en");
				break;
			case CafeConsoleLanguage::FR:
				strcpy(languageCode, "fr");
				break;
			case CafeConsoleLanguage::DE:
				strcpy(languageCode, "de");
				break;
			case CafeConsoleLanguage::IT:
				strcpy(languageCode, "it");
				break;
			case CafeConsoleLanguage::ES:
				strcpy(languageCode, "es");
				break;
			case CafeConsoleLanguage::ZH:
				strcpy(languageCode, "zh");
				break;
			case CafeConsoleLanguage::KO:
				strcpy(languageCode, "ko");
				break;
			case CafeConsoleLanguage::NL:
				strcpy(languageCode, "nl");
				break;
			case CafeConsoleLanguage::PT:
				strcpy(languageCode, "pt");
				break;
			case CafeConsoleLanguage::RU:
				strcpy(languageCode, "ru");
				break;
			case CafeConsoleLanguage::TW:
				strcpy(languageCode, "tw"); // usually zh-tw?
				break;
			default:
				strcpy(languageCode, "en");
				break;
			}

			const char* countryCode = NCrypto::GetCountryAsString(Account::GetCurrentAccount().GetCountry());

			char boss_code[0x20];
			strncpy(boss_code, (char*)&it->task_settings.settings[TaskSetting::kBossCode], TaskSetting::kBossCodeLen);

			sprintf(url, "https://npts.app.nintendo.net/p01/tasksheet/%s/%s/%s?c=%s&l=%s", "1", boss_code, it->task_id, countryCode, languageCode);
		}

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list_headerParam);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		forceLogDebug_printf("task_run url %s", url);

		int curl_result = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &it->http_status_code);

		//it->turn_state = kFinished;

		curl_slist_free_all(list_headerParam);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);

		if (curl_result != CURLE_OK)
		{
#ifndef PUBLIC_RELEASE
			forceLogDebug_printf("curl error buff: %s", errbuf);
#endif
			it->turn_state = kError;
			it->wait_state = TRUE;
			forceLogDebug_printf("task_run curl fail: %d", curl_result);
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_BOSS, 0);
		}
		else
		{
			if (it->http_status_code != 200)
			{
				forceLogDebug_printf("BOSS task_run: Received unexpected HTTP response code");
			}
			if (it->http_status_code == 404)
			{
				// todo - is this correct behavior?
				it->turn_state = kError;
				it->wait_state = TRUE;
				forceLogDebug_printf("task_run failed due to 404 error");
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_BOSS, 0);
			}
		}

		switch (it->content_type)
		{
		case ContentType::kXmlContent:
			parse_xml_content(*it);
			break;
		case ContentType::kBinaryFile:

			break;
		case ContentType::kText:
			forceLogDebug_printf("task_run returns text: %.*s", it->content_length, it->result_buffer.data());
			break;
		}


		if (!it->queued_files.empty())
		{
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, task_download_header_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, task_download_filecallback);

			std::string taskIdStr = it->task_id;

			try
			{
				fs::path path = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/data/{}", (uint32)(it->title_id >> 32), (uint32)(it->title_id & 0xFFFFFFFF), taskIdStr);
				if (!fs::exists(path))
					fs::create_directories(path);

				char targetFileName[TaskSetting::kFileNameLen + 1]{};
				strncpy(targetFileName, (char*)&it->task_settings.settings[TaskSetting::kNbdlFileName], TaskSetting::kFileNameLen);
				forceLogDebug_printf("\tnbdl task target filename: \"%s\"", targetFileName);
				const bool hasFileName = targetFileName[0] != '\0';

				while (!it->queued_files.empty())
				{
					auto file = it->queued_files.front();
					it->queued_files.pop();
					// download only specific file
					if (hasFileName && file.file_name != targetFileName)
						continue;

					it->processed_file_size = 0;
					it->file_buffer.clear();
					it->file_buffer.reserve(file.size);

					// create/open fad db content
					BossStorageFadFile fad_content{};
					fs::path db_file = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/{:08x}/{}", (uint32)(it->title_id >> 32), (uint32)(it->title_id & 0xFFFFFFFF), it->account_id, taskIdStr);
					if (!fs::exists(db_file))
						fs::create_directories(db_file);

					db_file /= "fad.db";
					std::ifstream fad_file(db_file, std::ios::in | std::ios::binary);
					if (fad_file.is_open())
					{
						if (!fad_file.read((char*)&fad_content, sizeof(BossStorageFadFile)))
							fad_content = {};

						fad_file.close();
					}

					auto currentEntry = boss_storage_fad_find_entry(fad_content, file.data_id);
					if(currentEntry)
					{
						uint64 timestamp = (uint64)currentEntry->timestampRelated + kTimeStampConvertSeconds;
						curl_easy_setopt(curl, CURLOPT_TIMEVALUE, timestamp);
						curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
					}
					else
					{
						curl_easy_setopt(curl, CURLOPT_TIMEVALUE, 0);
						curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
					}

					curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
					curl_easy_setopt(curl, CURLOPT_HEADERDATA, task_download_header_callback);
					curl_easy_setopt(curl, CURLOPT_URL, file.url.c_str());
					curl_result = curl_easy_perform(curl);
					if (curl_result != CURLE_OK)
					{
						forceLogDebug_printf("task_run curl failed on file download (%d): %s > %s", curl_result, file.file_name.c_str(), file.url.c_str());
						if (hasFileName)
						{
							turnstate = kError;
							break;
						}
						else
							continue;
					}

					long unmet = 1;
					const CURLcode result = curl_easy_getinfo(curl, CURLINFO_CONDITION_UNMET, &unmet);
					if(result == CURLE_OK && unmet == 1)
					{
						// file is already up2date
						if (hasFileName)
							break;
						else
							continue;
					}

					if(it->processed_file_size != file.size)
					{
						forceLogDebug_printf("task_run file download size mismatch: %s > %s > %d from %d bytes", file.file_name.c_str(), file.url.c_str(), it->processed_file_size, file.size);
						if (hasFileName)
						{
							turnstate = kError;
							break;
						}
						else
							continue;
					}

					uint64 filetime;
					curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime);

					// dunno about that one
					it->processed_length += it->processed_file_size;
					it->content_length += file.size;

					// bossAesKey = OTP.XorKey ^ bossXor
					// bossXor: 33 AC 6D 15 C2 26 0A 91 3B BF 73 C3 55 D8 66 04
					uint8 bossAesKey[16] = { 0x39,0x70,0x57,0x35,0x58,0x70,0x34,0x58,0x37,0x41,0x7a,0x30,0x71,0x5a,0x70,0x74 }; // "9pW5Xp4X7Az0qZpt"
					
					BossNbdlHeader* nbdlHeader = (BossNbdlHeader*)it->file_buffer.data();

					if (nbdlHeader->magic != 'boss')
						break;
					if (nbdlHeader->version != 0x20001)
						break;
					if (nbdlHeader->ukn08 != 1)
						break;
					if (nbdlHeader->ukn0A != 2)
						break;

					// file must be padded to 16 byte alignment for AES decryption (padding is cut off after decryption)
					const uint32 file_size = (it->processed_file_size + 0xF) & (~0xF);
					if (file_size != it->processed_file_size)
					{
						it->file_buffer.resize(file_size);
						nbdlHeader = (BossNbdlHeader*)it->file_buffer.data();
					}
					
					// prepare nonce for AES128-CTR
					uint8 aesNonce[0x10];
					memset(aesNonce, 0, sizeof(aesNonce));
					memcpy(aesNonce, nbdlHeader->nonce, 0xC);
					aesNonce[0xF] = 1;

					// decrypt header
					AES128CTR_transform(it->file_buffer.data() + offsetof(BossNbdlHeader, encryptedHeader), sizeof(BossNbdlHeader::encryptedHeader), bossAesKey, aesNonce);
					// decrypt everything else
					AES128CTR_transform(it->file_buffer.data() + sizeof(BossNbdlHeader), file_size - sizeof(BossNbdlHeader), bossAesKey, aesNonce);

					try
					{
						// create file with content
						fs::path file_path = path / fmt::format(L"{:08x}", file.data_id);
						std::ofstream new_file(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
						new_file.write((char*)it->file_buffer.data() + sizeof(BossNbdlHeader), it->processed_file_size - sizeof(BossNbdlHeader));
						new_file.flush();
						new_file.close();


						boss_storage_fad_append_or_update(fad_content, file.file_name.c_str(), file.data_id, filetime);
						std::ofstream fad_file_updated(db_file, std::ios::out | std::ios::binary | std::ios::trunc);
						fad_file_updated.write((char*)&fad_content, sizeof(BossStorageFadFile));
						fad_file_updated.flush();
						fad_file_updated.close();
					}
					catch (const std::exception& ex)
					{
						forceLogDebug_printf("file error: %s", ex.what());
					}

					if (hasFileName)
						break;
				}
			}
			catch (const std::exception& ex)
			{
				forceLogDebug_printf("dir error: %s", ex.what());
			}
		}


		if (it->task_settings.taskType == kRawDlTaskSetting)
		{
			char directoryName[TaskSetting::kDirectoryNameLen + 1]{};
			if (it->task_settings.settings[TaskSetting::kDirectoryName] != '\0')
				strncpy(directoryName, (char*)&it->task_settings.settings[TaskSetting::kDirectoryName], TaskSetting::kDirectoryNameLen);
			else
				strncpy(directoryName, it->task_id, TaskSetting::kDirectoryNameLen);

			char fileName[TaskSetting::kFileNameLen + 1]{};
			strncpy(fileName, (char*)&it->task_settings.settings[TaskSetting::kFileName], TaskSetting::kFileNameLen);

			//  mcl01\usr\boss\00050000\1018dd00\user\<persistentId>\<storageName>\<filename>
			fs::path path = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/{:08x}", (uint32)(it->title_id >> 32),
				(uint32)(it->title_id & 0xFFFFFFFF), iosuAct_getAccountIdOfCurrentAccount());
			path /= directoryName;

			if (!fs::exists(path))
				fs::create_directories(path);

			path /= fileName;

			std::ofstream file(path);
			if (file.is_open())
			{
				file.write((char*)it->result_buffer.data(), it->result_buffer.size());
				file.flush();
				file.close();
			}
		}

		it->turn_state = turnstate;
		it->wait_state = TRUE;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
	}


	bool task_is_registered(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = get_task(taskId, accountId, titleId);
		return it != g_boss.tasks.cend();
	}

	bool task_wait(const char* taskId, uint32 accountId, uint64 titleId, uint32 wait_state, uint32 timeout = 0)
	{
		const auto it = get_task(taskId, accountId, titleId);
		if (it == g_boss.tasks.cend())
		{
			return false;
		}

		const auto start = tick_cached();
		while (it->wait_state != wait_state)
		{
			if (timeout != 0 && (uint32)std::chrono::duration_cast<std::chrono::seconds>(tick_cached() - start).count() >= timeout)
			{
				forceLogDebug_printf("task_wait: timeout reached -> %d seconds passed", timeout);
				return false;
			}

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		return true;
	}

	uint32 task_register(const char* taskId, uint32 accountId, uint64 titleId, void* settings)
	{
		g_boss.tasks.emplace_back(taskId, accountId, titleId, (TaskSetting*)settings);
		g_boss.tasks[g_boss.tasks.size() - 1].turn_state = kWaitTime;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
	}

	uint32 task_register_immediate_run(const char* taskId, uint32 accountId, uint64 titleId, void* settings)
	{
		g_boss.tasks.emplace_back(taskId, accountId, titleId, (TaskSetting*)settings);
		g_boss.tasks[g_boss.tasks.size() - 1].turn_state = kWaitRun;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
	}

	void task_unregister(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = get_task(taskId, accountId, titleId);
		if (it != g_boss.tasks.cend())
			g_boss.tasks.erase(it);
	}

	std::pair<uint32, uint64> task_get_content_length(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = get_task(taskId, accountId, titleId);
		return it != g_boss.tasks.cend() ? std::make_pair(it->exec_count, it->content_length) : std::make_pair(0u, (uint64)0);
	}

	std::pair<uint32, uint64> task_get_processed_length(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = get_task(taskId, accountId, titleId);
		return it != g_boss.tasks.cend() ? std::make_pair(it->exec_count, it->processed_length) : std::make_pair(0u, (uint64)0);
	}

	std::pair<uint32, uint32> task_get_http_status_code(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = get_task(taskId, accountId, titleId);
		return it != g_boss.tasks.cend() ? std::make_pair(it->exec_count, it->http_status_code) : std::make_pair(0u, (uint32)0);
	}

	std::pair<uint32, uint32> task_get_turn_state(const char* taskId, uint32 accountId, uint64 titleId)
	{
		const auto it = get_task(taskId, accountId, titleId);
		return it != g_boss.tasks.cend() ? std::make_pair(it->exec_count, it->turn_state) : std::make_pair(0u, (uint32)0);
	}

	uint32 task_stop_scheduling(const char* task_id, uint32 account_id, uint64 title_id)
	{
		const auto it = get_task(task_id, account_id, title_id);
		if (it != g_boss.tasks.cend())
		{
			it->turn_state = kStopped;
			// todo actually cancel the task if currently running (curl call)
			// curl_easy_pause() -> resume on start scheduling if paused
		}

		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
	}

	void boss_thread()
	{
		SetThreadName("boss_thread");
		while (true)
		{
			const uint32 return_value = 0; // Ioctl return value

			ioQueueEntry_t* ioQueueEntry = iosuIoctl_getNextWithWait(IOS_DEVICE_BOSS);
			if (ioQueueEntry->request == IOSU_BOSS_REQUEST_CEMU)
			{
				iosuBossCemuRequest_t* cemu_request = (iosuBossCemuRequest_t*)ioQueueEntry->bufferVectors[0].buffer.GetPtr();
				cemu_request->returnCode = 0;

				uint64 title_id;
				if (cemu_request->titleId == 0)
					title_id = CafeSystem::GetForegroundTitleId();
				else
					title_id = cemu_request->titleId;

				uint32 account_id;
				if (cemu_request->accountId == 0)
					account_id = iosuAct_getAccountIdOfCurrentAccount();
				else
					account_id = cemu_request->accountId;

				if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_RUN)
				{
					cemu_request->returnCode = task_run(cemu_request->taskId, account_id, title_id);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_GET_CONTENT_LENGTH)
				{
					auto result = task_get_content_length(cemu_request->taskId, account_id, title_id);
					cemu_request->u64.exec_count = std::get<0>(result);
					cemu_request->u64.result = std::get<1>(result);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_GET_PROCESSED_LENGTH)
				{
					auto result = task_get_processed_length(cemu_request->taskId, account_id, title_id);
					cemu_request->u64.exec_count = std::get<0>(result);
					cemu_request->u64.result = std::get<1>(result);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_GET_HTTP_STATUS_CODE)
				{
					auto result = task_get_http_status_code(cemu_request->taskId, account_id, title_id);
					cemu_request->u32.exec_count = std::get<0>(result);
					cemu_request->u32.result = std::get<1>(result);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_GET_TURN_STATE)
				{
					auto result = task_get_turn_state(cemu_request->taskId, account_id, title_id);
					cemu_request->u32.exec_count = std::get<0>(result);
					cemu_request->u32.result = std::get<1>(result);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_WAIT)
				{
					cemu_request->returnCode = task_wait(cemu_request->taskId, account_id, title_id, cemu_request->waitState,
					                                     cemu_request->timeout);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_REGISTER)
				{
					cemu_request->returnCode = task_register(cemu_request->taskId, account_id, title_id, cemu_request->settings);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_IS_REGISTERED)
				{
					cemu_request->returnCode = task_is_registered(cemu_request->taskId, account_id, title_id) ? TRUE : FALSE;
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_REGISTER_FOR_IMMEDIATE_RUN)
				{
					cemu_request->returnCode = task_register_immediate_run(cemu_request->taskId, account_id, title_id,
					                                                       cemu_request->settings);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_UNREGISTER)
				{
					task_unregister(cemu_request->taskId, account_id, title_id);
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_START_SCHEDULING)
				{
					// we just run it no matter what
					//if(cemu_request->bool_parameter)
						cemu_request->returnCode = task_run(cemu_request->taskId, account_id, title_id);
					/*else
					{
						const auto it = get_task(cemu_request->taskId, account_id, title_id);
						if (it != g_boss.tasks.cend())
						{
							it->turn_state = kWaitRun;
						}
					}*/
				}
				else if (cemu_request->requestCode == IOSU_NN_BOSS_TASK_STOP_SCHEDULING)
				{
					cemu_request->returnCode = task_stop_scheduling(cemu_request->taskId, account_id, title_id);
				}
				else
					assert_dbg();
			}
			else
				assert_dbg();

			iosuIoctl_completeRequest(ioQueueEntry, return_value);
		}
	}

	void boss_init()
	{
		if (g_boss.is_initialized)
			return;

		// start the boss thread
		std::thread t(boss_thread);
		t.detach();

		g_boss.is_initialized = true;
	}
}
