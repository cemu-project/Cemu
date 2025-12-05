#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "util/helpers/helpers.h"
#include "Cafe/Filesystem/fsc.h"

#include "Cafe/IOSU/iosu_types_common.h"
#include "Cafe/IOSU/nn/iosu_nn_service.h"

#include "Cafe/IOSU/legacy/iosu_act.h"
#include "Cafe/CafeSystem.h"
#include "config/ActiveSettings.h"

#include "boss_service.h"
#include "boss_common.h"

#include <pugixml.hpp>
#include <curl/curl.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "Cemu/ncrypto/ncrypto.h"
#include "Cemu/napi/napi_helper.h"
#include "IOSU/legacy/iosu_crypto.h"
#include "util/crypto/aes128.h"

namespace iosu::boss
{
	using namespace ::nn::boss;

	static constexpr nnResult RESULT_SUCCESS = 0x200080;
	static constexpr nnResult RESULT_C0203880 = 0xC0203880;
	static constexpr nnResult RESULT_NOTEXIST = 0xA021F900;

	static constexpr nnResult RESULT_STORAGE_NOTEXIST = 0xA025AA00;
	static constexpr nnResult RESULT_FADENTRY_NOTEXIST = 0xA021FB00;

	template <typename ... TArgs>
	void AppendHeaderParam(CurlRequestHelper& request, const char* fieldName, const char* format, TArgs&& ... args)
	{
		request.addHeaderField(fieldName, fmt::format(fmt::runtime(format), std::forward<TArgs>(args)...).c_str());
	}

	class StorageDatabase // FAD
	{
		static constexpr uint32 FAD_ENTRY_MAX_COUNT = 512; // max entries in a single FAD file

	public:
		struct BossStorageFadEntry
		{
			CafeString<32> fileName; // 0x00
			uint32be fileId; // 0x20
			uint32 ukn24; // 0x24
			uint32 flags; // 0x28
			uint32 memo_2C; // 0x2C
			uint64be entryCreationTimestamp;
			// flags:
			// 0x80000000		ReadFlag
			// 0x40000000		DeleteProtectionFlag
			// 0x20000000		New arrival flag?
		};
		static_assert(sizeof(BossStorageFadEntry) == 0x38);

		struct BossStorageFadFile
		{
			uint8 _00[0x08];
			BossStorageFadEntry entries[FAD_ENTRY_MAX_COUNT];
		};
		static_assert(sizeof(BossStorageFadFile) == 28680);

		static bool CheckIfStorageExists(const DirectoryName& bossDirectory, uint64 titleId, uint32 persistentId)
		{
			fs::path fadPath = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/{:08x}/{}", GetTitleIdHigh(titleId), GetTitleIdLow(titleId), persistentId, bossDirectory.name2.c_str());
			std::error_code ec;
			return fs::exists(fadPath, ec);
		}

		nnResult Load(TaskId& taskId, uint64 titleId, uint32 persistentId)
		{
			// todo - if same FAD db already loaded then skip this
			m_hasValidFadData = false;
			memset(&m_fadData, 0, sizeof(m_fadData));
			fs::path fadPath = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/{:08x}/{}", GetTitleIdHigh(titleId), GetTitleIdLow(titleId), persistentId, taskId.id.c_str());
			std::error_code ec;
			if (!fs::exists(fadPath, ec))
				fs::create_directories(fadPath, ec);
			fadPath /= "fad.db";
			m_fadFilePath = fadPath;
			FileStream* fs = FileStream::openFile2(fadPath);
			if (!fs)
				return RESULT_STORAGE_NOTEXIST;
			fs->readData(&m_fadData, sizeof(m_fadData));
			delete fs;
			m_hasValidFadData = true; // the file may not exist yet, so consider it valid even if we cant open it
			return RESULT_SUCCESS;
		}

		nnResult CreateNewStorage(TaskId& taskId, uint64 titleId, uint32 persistentId)
		{
			memset(&m_fadData, 0, sizeof(m_fadData));
			fs::path fadPath = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/{:08x}/{}", GetTitleIdHigh(titleId), GetTitleIdLow(titleId), persistentId, taskId.id.c_str());
			std::error_code ec;
			if (!fs::exists(fadPath, ec))
				fs::create_directories(fadPath, ec);
			m_fadFilePath = fadPath / "fad.db";
			FileStream* fs = FileStream::createFile2(m_fadFilePath);
			if (fs)
			{
				fs->writeData(&m_fadData, sizeof(m_fadData));
				delete fs;
				m_hasValidFadData = true;
			}
			else
			{
				cemuLog_log(LogType::Force, "Failed to create FAD data file at {}", _pathToUtf8(m_fadFilePath));
				return NN_RESULT_PLACEHOLDER_ERROR;
			}
			return RESULT_SUCCESS;
		}

		void Store()
		{
			if (!m_hasValidFadData)
				return;
			FileStream* fs = FileStream::createFile2(m_fadFilePath);
			if (fs)
			{
				fs->writeData(&m_fadData, sizeof(m_fadData));
				delete fs;
			}
			else
			{
				cemuLog_log(LogType::Force, "Failed to store FAD data to {}", m_fadFilePath.string());
			}
		}

		void Clear()
		{
			m_hasValidFadData = false;
			memset(&m_fadData, 0, sizeof(m_fadData));
		}

		void DeleteEntryByFileName(CafeString<32> fileName)
		{
			cemu_assert_debug(m_hasValidFadData);
			for (sint32 i = 0; i < FAD_ENTRY_MAX_COUNT; i++)
			{
				if (m_fadData.entries[i].fileName == fileName)
				{
					// shift remaining entries
					memmove(m_fadData.entries + i, m_fadData.entries + i + 1, (FAD_ENTRY_MAX_COUNT - i - 1) * sizeof(BossStorageFadEntry));
					// reset last entry
					memset(m_fadData.entries + FAD_ENTRY_MAX_COUNT - 1, 0, sizeof(BossStorageFadEntry));
					return;
				}
			}
		}

		nnResult AddEntry(CafeString<32> fileName, uint32 fileId)
		{
			if (!fileId)
			{
				cemuLog_log(LogType::Force, "Cannot add BOSS file with fileId 0");
				return false;
			}
			if (fileName.empty())
				return false;
			DeleteEntryByFileName(fileName);
			cemu_assert_debug(m_hasValidFadData);
			for (sint32 i=0; i<FAD_ENTRY_MAX_COUNT; i++)
			{
				if (m_fadData.entries[i].fileId == 0)
				{
					m_fadData.entries[i].fileName = fileName;
					m_fadData.entries[i].fileId = fileId;
					m_fadData.entries[i].memo_2C = 0;
					m_fadData.entries[i].entryCreationTimestamp = 0;
					m_fadData.entries[i].ukn24 = 0;
					m_fadData.entries[i].flags = 0;
					return 0x200080;
				}
			}
			cemuLog_log(LogType::Force, "FAD file {} is full", _pathToUtf8(m_fadFilePath));
			return 0xA021FB00;
		}

		void GetDataList(std::vector<DataName>& dataList)
		{
			cemu_assert_debug(m_hasValidFadData);
			dataList.clear();
			for (sint32 i = 0; i < FAD_ENTRY_MAX_COUNT; i++)
			{
				if (m_fadData.entries[i].fileId == 0)
					break; // end of list
				DataName dataName;
				dataName.name = m_fadData.entries[i].fileName;
				dataList.push_back(dataName);
			}
		}

		bool GetEntryByFilename(const DataName& fileName, BossStorageFadEntry& outEntry)
		{
			if (fileName.name.empty())
				return false;
			cemu_assert_debug(m_hasValidFadData);
			for (sint32 i = 0; i < FAD_ENTRY_MAX_COUNT; i++)
			{
				if (m_fadData.entries[i].fileName == fileName.name)
				{
					outEntry = m_fadData.entries[i];
					return true;
				}
				if (m_fadData.entries[i].fileId == 0)
					break; // end of list
			}
			return false;
		}

	private:
		BossStorageFadFile m_fadData;
		bool m_hasValidFadData = false;
		fs::path m_fadFilePath;
	};
	StorageDatabase m_fadDb;

	class NsDataAccessor
	{
	public:
		nnResult Open(const DirectoryName& bossDirectory, uint64 titleId, uint32 persistentId, DataName& fileName)
		{
			m_isValid = false;
			m_nsDataPath.clear();
			TaskId taskId(bossDirectory.name2.c_str());
			nnResult r = m_fadDb.Load(taskId, titleId, persistentId);
			if (NN_RESULT_IS_FAILURE(r))
				return r;
			if (!m_fadDb.GetEntryByFilename(fileName, m_fadEntry))
				return RESULT_FADENTRY_NOTEXIST;
			m_nsDataPath = BuildNsDataPath(bossDirectory, titleId, persistentId, m_fadEntry.fileId);
			m_isValid = true;
			m_bossDirectory = bossDirectory;
			m_titleId = titleId;
			m_persistentId = persistentId;
			m_fileName = fileName;
			return RESULT_SUCCESS;
		}

		void Close()
		{
			m_isValid = false;
		}

		nnResult Delete()
		{
			cemu_assert_debug(m_isValid); // only call this after successful Open()
			std::error_code ec;
			if (!fs::remove(m_nsDataPath, ec) )
				cemuLog_log(LogType::Force, "Failed to delete BOSS file {}", _pathToUtf8(m_nsDataPath));
			// remove from FAD
			TaskId taskId(m_bossDirectory.name2.c_str());
			nnResult r = m_fadDb.Load(taskId, m_titleId, m_persistentId);
			if (NN_RESULT_IS_SUCCESS(r))
				m_fadDb.DeleteEntryByFileName(m_fileName.name);
			m_isValid = false;
			return RESULT_SUCCESS;
		}

		bool Exists() const
		{
			if (!m_isValid)
				return false;
			// check if the file actually exists on the host filesystem
			std::error_code ec;
			if (m_nsDataPath.empty())
				return false;
			bool r = fs::exists(m_nsDataPath, ec);
			if (!r)
				cemuLog_log(LogType::Force, "BOSS: File exists in FAD cache but not on disk {}. To fix this reset the SpotPass cache. In the menu under debug select \"Clear SpotPass cache\"", _pathToUtf8(m_nsDataPath));
			return r;
		}

		nnResult GetSize(uint32& fileSize) const
		{
			cemu_assert_debug(m_isValid); // only call this after successful Open()
			if (!m_isValid)
				return NN_RESULT_PLACEHOLDER_ERROR;
			std::unique_ptr<FileStream> fs(FileStream::openFile2(m_nsDataPath));
			if (!fs)
				return NN_RESULT_PLACEHOLDER_ERROR;
			fileSize = (uint32)fs->GetSize();
			return RESULT_SUCCESS;
		}

		bool Read(void* buffer, uint32 size, uint32 offset, uint32& bytesRead)
		{
			if (!m_isValid)
				return false;
			std::unique_ptr<FileStream> fs(FileStream::openFile2(m_nsDataPath));
			if (!fs)
				return false;
			fs->SetPosition(offset);
			bytesRead = (uint32)fs->readData(buffer, size);
			return true;
		}

	private:
		fs::path BuildNsDataPath(const DirectoryName& bossDirectory, uint64 titleId, uint32 persistentId, uint32 dataId)
		{
			return ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/data/{}/{:08x}", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF), bossDirectory.name2.c_str(), dataId);
		}

		bool m_isValid{false};
		StorageDatabase::BossStorageFadEntry m_fadEntry;
		fs::path m_nsDataPath;
		// if m_isValid is true, these hold the current file information:
		DirectoryName m_bossDirectory;
		uint64 m_titleId{};
		uint32 m_persistentId{};
		DataName m_fileName;
	};

	NsDataAccessor m_nsDataAccessor;

	class RegisteredTask
	{
	public:
		RegisteredTask(const TaskId& taskId, const TaskSettingCore& taskSettings)
			: m_taskId(taskId), m_taskSettings(taskSettings)
		{
			m_persistentId = iosuAct_getAccountIdOfCurrentAccount();
		}

		nnResult Run()
		{
			std::unique_lock _l(m_mutex);
			cemu_assert_debug(m_taskState == TaskState::Initial || m_taskState == TaskState::Done);
			m_taskTurnState = TaskTurnState::Ready;
			m_taskState = TaskState::Ready;
			return RESULT_SUCCESS;
		}

		TaskState GetState()
		{
			std::unique_lock _l(m_mutex);
			return m_taskState;
		}

		TaskTurnState GetTurnState()
		{
			std::unique_lock _l(m_mutex);
			return m_taskTurnState;
		}

		sint32 GetHttpStatusCode() const
		{
			return m_httpStatusCode;
		}

		uint32 GetContentLength() const
		{
			return m_contentLength;
		}

		uint32 GetProcessedLength() const
		{
			return m_contentLength; // todo - unlike content length, this value is getting updated as the download happens. But for now we just return the content length
		}

		nnResult TaskDoRequest(CURL* curl)
		{
			std::unique_lock _l(m_mutex);
			if (m_taskState != TaskState::Ready)
			{
				cemuLog_log(LogType::Force, "Task {} is not ready to run, current state: {}", m_taskId.id.c_str(), static_cast<uint32>(m_taskState));
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_BOSS, 0);
			}
			m_taskState = TaskState::Running;
			m_taskTurnState = TaskTurnState::Running;
			m_contentLength = 0;
			_l.unlock();
			if (!ActiveSettings::IsOnlineEnabled())
			{
				m_taskState = TaskState::Done;
				m_taskTurnState = TaskTurnState::DoneError;
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
			}
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			// construct request URL
			std::string requestUrl;
			if (!m_taskSettings.url.empty())
				requestUrl.assign(m_taskSettings.url.c_str()); // custom url
			else
			{
				switch (ActiveSettings::GetNetworkService())
				{
				case NetworkService::Pretendo:
					requestUrl = PretendoURLs::BOSSURL;
					break;
				case NetworkService::Custom:
					requestUrl = GetNetworkConfig().urls.BOSS.GetValue();
					break;
				case NetworkService::Nintendo:
				default:
					requestUrl = NintendoURLs::BOSSURL;
					break;
				}
			}
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
			if (m_taskSettings.taskType == TaskType::NbdlDataListTaskSetting)
			{
				// use bossCode (and fileName?) as part of the URL
				requestUrl.append(fmt::format(fmt::runtime("/{}/{}/{}?c={}&l={}"), "1", m_taskSettings.nbdl.bossCode.c_str(), m_taskId.id.c_str(), countryCode, languageCode));
				cemu_assert_unimplemented();
				cemuLog_logDebug(LogType::Force, "IOSU-BOSS: Unsupported task type: {}", static_cast<uint32>(m_taskSettings.taskType.value()));
				_l.lock();
				m_taskState = TaskState::Done;
				m_taskTurnState = TaskTurnState::DoneError;
				m_contentLength = 0;
				m_httpStatusCode = 0;
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
			}
			else if (m_taskSettings.taskType == TaskType::NbdlTaskSetting)
			{
				// todo - language and country code come from the task settings
				cemu_assert_debug(m_taskSettings.nbdl.fileName.empty()); // todo - changes the url
				const char* apiVersion = "1";
				requestUrl.append(fmt::format(fmt::runtime("/{}/{}/{}?c={}&l={}"), apiVersion, m_taskSettings.nbdl.bossCode.c_str(), m_taskId.id.c_str(), countryCode, languageCode));
			}
			else if (m_taskSettings.taskType == TaskType::PlayReportSetting)
			{
				cemuLog_logDebug(LogType::Force, "IOSU-BOSS: PlayReport tasks are not implemented yet");
				_l.lock();
				m_taskState = TaskState::Done;
				m_taskTurnState = TaskTurnState::DoneError;
				m_contentLength = 0;
				m_httpStatusCode = 200;
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
			}
			else
			{
				cemuLog_logDebug(LogType::Force, "IOSU-BOSS: Unknown task type: {}", static_cast<uint32>(m_taskSettings.taskType.value()));
				_l.lock();
				m_taskState = TaskState::Done;
				m_taskTurnState = TaskTurnState::DoneError;
				m_contentLength = 0;
				m_httpStatusCode = 0;
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
			}

			CurlRequestHelper request;
			request.initate(ActiveSettings::GetNetworkService(), requestUrl, CurlRequestHelper::SERVER_SSL_CONTEXT::CUSTOM);
			// add certs
			SetupSSL(request);
			// parameters
			AppendHeaderParam(request, "X-BOSS-Digest:", ""); // todo - implement this
			AppendHeaderParam(request, "X-Boss-UniqueId", "{:05x}", ((titleId >> 8) & 0xFFFFF));
			AppendHeaderParam(request, "X-BOSS-TitleId", "{:016x}", titleId);
			if (!m_taskSettings.serviceToken.empty())
				AppendHeaderParam(request, "X-Nintendo-ServiceToken", "{}", m_taskSettings.serviceToken.c_str());

			if (!request.submitRequest(false))
			{
				cemuLog_log(LogType::Force, fmt::format("Failed BOSS request"));
				_l.lock();
				m_taskState = TaskState::Done;
				m_taskTurnState = TaskTurnState::DoneError;
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_BOSS, 0);
			}
			sint32 httpStatusCode = request.GetHTTPStatusCode();
			m_httpStatusCode = httpStatusCode;
			m_contentLength = request.getReceivedData().size(); // note - for Nbdl tasks which length is returned?

			{
				if (httpStatusCode != 200)
				{
					cemuLog_logDebug(LogType::Force, "BOSS task_run: Received unexpected HTTP response code {}", httpStatusCode);
					cemuLog_logDebug(LogType::Force, "URL: {}", requestUrl);
				}
				if (httpStatusCode == 404)
				{
					_l.lock();
					m_taskState = TaskState::Done;
					m_taskTurnState = TaskTurnState::DoneError;
					cemuLog_logDebug(LogType::Force, "BOSS task_run: Received 404 Not Found for URL: {}", requestUrl);
					return BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_BOSS, 0);
				}
			}

			// for Nbdl tasks get the list of files to download
			if (m_taskSettings.taskType == TaskType::NbdlTaskSetting)
			{
				std::vector<NbdlQueuedFile> queuedFiles = ParseNbdlTasksheet(request.getReceivedData());
				cemuLog_log(LogType::Force, "SpotPass - NbdlTask has {} files to download:", queuedFiles.size());
				for (const auto& file : queuedFiles)
				{
					DownloadNbdlFile(file);
				}
			}

			if (m_taskSettings.taskType == TaskType::RawDlTaskSetting_1 || m_taskSettings.taskType == TaskType::RawDlTaskSetting_3 || m_taskSettings.taskType == TaskType::RawDlTaskSetting_9)
			{
				cemu_assert_unimplemented();
			}
			{
				_l.lock();
				m_taskState = TaskState::Done;
				m_taskTurnState = TaskTurnState::DoneSuccess;
			}
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
		}

		std::recursive_mutex& GetMutex() { return m_mutex; }
	private:
		struct NbdlQueuedFile
		{
			NbdlQueuedFile(std::string_view url, std::string_view fileName, uint32 dataId, uint32 fileType, uint32 size)
				: url(url), fileName(fileName), dataId(dataId), fileType(fileType), size(size)
			{}

			std::string url;
			std::string fileName;
			uint32 dataId;
			uint32 fileType; // 0 = AppData, 1 = Message, 2 = Unknown
			uint32 size;
		};

		std::vector<NbdlQueuedFile> ParseNbdlTasksheet(std::span<uint8> xmlContent)
		{
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_buffer(xmlContent.data(), xmlContent.size());
			if (!result)
				return {};
			std::vector<NbdlQueuedFile> fileList;
			for (pugi::xml_node sheet = doc.child("TaskSheet"); sheet; sheet = sheet.next_sibling("TaskSheet"))
			{
				pugi::xml_node serviceStatus = sheet.child("ServiceStatus");
				if (strcmp(serviceStatus.child_value(), "close") == 0)
				{
					// service is closed, skip all files
					cemuLog_log(LogType::Force, "BossNbdl: Service is closed, skipping all files in tasksheet");
					continue;
				}
				else if (strcmp(serviceStatus.child_value(), "open") != 0)
				{
					cemuLog_log(LogType::Force, "BossNbdl: Unknown ServiceStatus value: {}", serviceStatus.child_value());
				}
				// read and verify titleId
				pugi::xml_node nodeTitleId = sheet.child("TitleId");
				if (nodeTitleId)
				{
					uint64 titleId;
					TitleIdParser::ParseFromStr(nodeTitleId.child_value(), titleId);
					if (titleId != CafeSystem::GetForegroundTitleId())
						cemuLog_log(LogType::Force, "BossNbdl: TitleId in tasksheet ({:016x}) does not match foreground titleId ({:016x})", titleId, CafeSystem::GetForegroundTitleId());
				}
				else
				{
					cemuLog_log(LogType::Force, "BossNbdl: Missing TitleId field in tasksheet");
				}
				// read and verify taskId
				pugi::xml_node nodeTaskId = sheet.child("TaskId");
				if (nodeTaskId)
				{
					if (strcmp(nodeTaskId.child_value(), m_taskId.id.c_str()) != 0)
						cemuLog_log(LogType::Force, "BossNbdl: TaskId in tasksheet ({}) does not match current taskId ({})", nodeTaskId.child_value(), m_taskId.id.c_str());
				}
				else
				{
					cemuLog_log(LogType::Force, "BossNbdl: Missing TaskId field in tasksheet");
				}

				// read files list
				pugi::xml_node files = sheet.child("Files");
				if (!files)
					continue;
				for (pugi::xml_node file = files.child("File"); file; file = file.next_sibling("File"))
				{
					pugi::xml_node fileName = file.child("Filename");
					if (!fileName)
						continue;
					pugi::xml_node dataId = file.child("DataId");
					if (!dataId)
						continue;
					pugi::xml_node url = file.child("Url");
					if (!url)
						continue;
					pugi::xml_node size = file.child("Size");
					if (!size)
						continue;
					pugi::xml_node type = file.child("Type");
					if (!type)
						continue;
					uint32 fileType;
					if (strcmp(type.child_value(), "AppData") == 0)
						fileType = 0;
					else if (strcmp(type.child_value(), "Message") == 0)
						fileType = 1;
					else
					{
						cemuLog_log(LogType::Force, "BossNbdl: Unknown file Type value: {}", type.child_value());
						fileType = 0;
					}

					if (!file.child("AutoDelete").empty())
						cemuLog_log(LogType::Force, "BossNbdl: Field AutoDelete is set but not supported. Value: {}", file.child_value("AutoDelete"));

					// fields and categories which we dont handle yet:
					// Attributes, Attribute1, Attribute2, Attribute3
					// Notify, Conditions, SecInterval, LED, Played

					fileList.emplace_back(
						url.child_value(),
						fileName.child_value(),
						dataId.text().as_int(),
						fileType,
						size.text().as_int()
					);
				}
			}
			return fileList;
		}

		void SetupSSL(CurlRequestHelper& request)
		{
			request.ClearCaCertIds();
			request.ClearClientCertIds();
			for (sint32 i=0; i<3; i++)
			{
				if (m_taskSettings.internalCaCert[i] != 0)
					request.AddCaCertId(m_taskSettings.internalCaCert[i]);
			}
			request.AddCaCertId(105);
			if (m_taskSettings.internalClientCert != 0)
				request.AddCaCertId(m_taskSettings.internalClientCert);
			else
				request.AddCaCertId(3); // whats the default for each task type?
			cemu_assert_debug(m_taskSettings.caCert[0].empty());
			cemu_assert_debug(m_taskSettings.caCert[1].empty());
			cemu_assert_debug(m_taskSettings.caCert[2].empty());
			cemu_assert_debug(m_taskSettings.clientCertName.empty());
		}

		void DownloadNbdlFile(const NbdlQueuedFile& nbdlFile)
		{
			uint64 titleId = CafeSystem::GetForegroundTitleId(); // todo - use titleId from task settings?
			fs::path dataFilePath = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/data/{}/{:08x}", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF), m_taskId.id.c_str(), nbdlFile.dataId);

			// if the file already exists locally, skip download
			// but still add the entry to the FAD db
			std::error_code ec;
			if (fs::exists(dataFilePath))
			{
				cemuLog_log(LogType::Force, "\t- {} (DataId:{:08x} {}KB) (Skipping, already downloaded)", nbdlFile.fileName, nbdlFile.dataId, (nbdlFile.size+1023)/1024);
				TrackDownloadedNbdlFile(nbdlFile);
				return;
			}
			cemuLog_log(LogType::Force, "\t- {} (DataId:{:08x} {}KB)", nbdlFile.fileName, nbdlFile.dataId, (nbdlFile.size+1023)/1024);

			CurlRequestHelper request;
			request.initate(ActiveSettings::GetNetworkService(), nbdlFile.url, CurlRequestHelper::SERVER_SSL_CONTEXT::CUSTOM);
			SetupSSL(request);

			bool r = request.submitRequest();
			if (!r)
			{
				cemuLog_log(LogType::Force, "Failed to download SpotPass nbdl file: {}", nbdlFile.fileName);
				return;
			}

			// decrypt and store file
			std::vector<uint8> receivedData = request.getReceivedData();
			struct BossNbdlHeader
			{
				/* +0x00 */ uint32be magic;
				/* +0x04 */	uint32be version; // guessed
				/* +0x08 */	uint16be ukn08; // must always be 1
				/* +0x0A */	uint16be ukn0A; // must always be 2
				/* +0x0C */	uint8 nonce[0xC];
				/* +0x18 */	uint32 padding18; // unused
				/* +0x1C */	uint32 padding1C; // unused
				/* +0x20 */
				struct
				{
					uint8 uknHashData[0x20];
				} encryptedHeader;
			};
			if (receivedData.size() < sizeof(BossNbdlHeader))
			{
				cemuLog_log(LogType::Force, "Received SpotPass nbdl file is too small: {} bytes", receivedData.size());
				return;
			}
			BossNbdlHeader nbdlHeader;
			memcpy(&nbdlHeader, receivedData.data(), sizeof(BossNbdlHeader));
			if (nbdlHeader.magic != 'boss' || nbdlHeader.version != 0x20001 || nbdlHeader.ukn08 != 1 || nbdlHeader.ukn0A != 2)
			{
				cemuLog_log(LogType::Force, "Received SpotPass nbdl file has incorrect header");
				return;
			}
			// file must be padded to 16 byte alignment for AES decryption (padding is cut off after decryption)
			size_t originalFileSize = receivedData.size();
			const uint32 paddedFileSize = (originalFileSize + 0xF) & (~0xF);
			receivedData.resize(paddedFileSize);
			// extra validation
			if (originalFileSize != nbdlFile.size)
			{
				cemuLog_log(LogType::Force, "SpotPass nbdl file size mismatch, expected {} bytes but got {} bytes", nbdlFile.size, originalFileSize);
				return;
			}
			if (nbdlFile.fileName.find("..") != std::string::npos)
			{
				cemuLog_log(LogType::Force, "SpotPass nbdl filename contains suspicious characters");
				return;
			}
			// prepare nonce for AES128-CTR
			uint8 aesNonce[0x10];
			memset(aesNonce, 0, sizeof(aesNonce));
			memcpy(aesNonce, nbdlHeader.nonce, 0xC);
			aesNonce[0xF] = 1;
			// decrypt
			uint8 bossAesKey[16] = { 0x39,0x70,0x57,0x35,0x58,0x70,0x34,0x58,0x37,0x41,0x7a,0x30,0x71,0x5a,0x70,0x74 };
			memset(aesNonce, 0, sizeof(aesNonce));
			memcpy(aesNonce, nbdlHeader.nonce, 0xC);
			aesNonce[0xF] = 1;
			AES128CTR_transform(receivedData.data() + offsetof(BossNbdlHeader, encryptedHeader), receivedData.size() - sizeof(BossNbdlHeader::encryptedHeader), bossAesKey, aesNonce);
			// get HMAC from header
			uint8 fileHMAC[32];
			memcpy(fileHMAC, &((BossNbdlHeader*)receivedData.data())->encryptedHeader.uknHashData, 32);
			// write decrypted data to filesystem using a temporary path
			fs::path tmpDataPath = ActiveSettings::GetMlcPath("usr/boss/{:08x}/{:08x}/user/common/data/{}/.{:08x}", (uint32)(titleId >> 32), (uint32)(titleId & 0xFFFFFFFF), m_taskId.id.c_str(), nbdlFile.dataId);
			if (!fs::exists(tmpDataPath.parent_path()))
				fs::create_directories(tmpDataPath.parent_path(), ec);
			FileStream* fs = FileStream::createFile2(tmpDataPath);
			if (fs)
			{
				fs->writeData(receivedData.data() + sizeof(BossNbdlHeader), originalFileSize - sizeof(BossNbdlHeader));
				delete fs;
			}
			else
			{
				cemuLog_log(LogType::Force, "Failed to create temporary SpotPass nbdl file: {}", _pathToUtf8(tmpDataPath));
				return;
			}
			// to make sure that the file actually stored correctly to disk we read it back and verify the hash
			auto readbackFileData = FileStream::LoadIntoMemory(tmpDataPath);
			uint8 hmacHash[32];
			unsigned int hmacHashLen = 32;
			HMAC(EVP_sha256(), "uyr5I8pu4ycq2pOT3D53Bp0n7jK8eyjLO5U20ocUNdN5muwUcC4By881UXECeM08", 64, readbackFileData->data(), originalFileSize - sizeof(BossNbdlHeader), hmacHash, &hmacHashLen);
			if (memcmp(hmacHash, fileHMAC, 32) != 0)
			{
				// failed to verify hash
				cemuLog_log(LogType::Force, "Failed to verify hash for SpotPass nbdl file: {}", _pathToUtf8(tmpDataPath));
				fs::remove(tmpDataPath, ec);
				return;
			}
			// rename temporary file to final path
			fs::rename(tmpDataPath, dataFilePath, ec);
			// update FAD entry
			TrackDownloadedNbdlFile(nbdlFile);
		}

		void TrackDownloadedNbdlFile(const NbdlQueuedFile& nbdlFile)
		{
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			nnResult storageResult = m_fadDb.Load(m_taskId, titleId, m_persistentId);
			if (storageResult == RESULT_STORAGE_NOTEXIST)
				storageResult = m_fadDb.CreateNewStorage(m_taskId, titleId, m_persistentId);
			if (NN_RESULT_IS_FAILURE(storageResult))
			{
				cemuLog_log(LogType::Force, "Failed to create FAD database for task {}: {:08x}", m_taskId.id.c_str(), storageResult);
				return;
			}
			CafeString<32> filenameStr;
			filenameStr.assign(nbdlFile.fileName);
			m_fadDb.AddEntry(filenameStr, nbdlFile.dataId);
			m_fadDb.Store();
			// todo - DIDX and ref database
		}

		TaskId m_taskId;
		uint32 m_persistentId;
		TaskSettingCore m_taskSettings;
		std::recursive_mutex m_mutex;
		TaskState m_taskState{ TaskState::Initial };
		TaskTurnState m_taskTurnState{ TaskTurnState::Ukn };
		sint32 m_httpStatusCode{ 0 };
		uint32 m_contentLength{ 0 }; // content length of the last request
	};

	class BossDaemon
	{
	public:
		void Start()
		{
			if (m_threadRunning.exchange(true))
				return;
			m_bossDaemonThread = std::thread(&BossDaemon::BossDaemonThread, this);
		}

		void Stop()
		{
			if (!m_threadRunning.exchange(false))
				return;
			m_bossDaemonThread.join();
		}

		void RegisterTask(const TaskSettingCore& taskSetting)
		{
			std::unique_lock _l(m_taskMtx);
			if (m_registeredTasks.find(taskSetting.taskId) != m_registeredTasks.end())
			{
				// task already registered. Return error?
				cemuLog_logDebug(LogType::Force, "IOSU-Boss: Task {} is already registered", taskSetting.taskId.id.c_str());
				return;
			}
			m_registeredTasks.try_emplace(taskSetting.taskId, std::make_shared<RegisteredTask>(taskSetting.taskId, taskSetting));
		}

		void UnregisterTask(uint32 persistentId, const TaskId& taskId)
		{
			std::unique_lock _l(m_taskMtx);
			auto it = m_registeredTasks.find(taskId);
			if (it != m_registeredTasks.end())
			{
				m_registeredTasks.erase(it);
			}
			else
			{
				// todo - return error
			}
		}

		bool TaskIsRegistered(uint32 persistentId, const TaskId& taskId)
		{
			std::unique_lock _l(m_taskMtx);
			return m_registeredTasks.find(taskId) != m_registeredTasks.end();
		}

		void TaskRun(uint32 persistentId, const TaskId& taskId)
		{
			// todo - there is an extra parameter for controlling background/foreground running?
			std::shared_ptr<RegisteredTask> registeredTask = GetRegisteredTask2(persistentId, taskId);
			if (registeredTask)
			{
				cemuLog_log(LogType::Force, "IOSU-Boss: Running task {}", taskId.id.c_str());
				registeredTask->Run();
			}
			else
			{
				cemuLog_log(LogType::Force, "IOSU-Boss: Trying to run task {} which has not been registered", taskId.id.c_str());
				// todo - return error -> 0xA021F900
			}
		}

		void StartScheduleTask(uint32 persistentId, const TaskId& taskId, bool runImmediately)
		{
			if (runImmediately)
			{
				// proper implementation is still todo
				// handling automatic scheduling and task state transitions is quite complex
				// for now we only run the task once
				std::shared_ptr<RegisteredTask> registeredTask = GetRegisteredTask2(persistentId, taskId);
				if(!registeredTask)
					return;
				TaskState state = registeredTask->GetState();
				if (state == TaskState::Stopped || state == TaskState::Initial)
				{
					registeredTask->Run();
				}
				else
				{
					cemuLog_logDebug(LogType::Force, "StartScheduleTask(): No support for scheduling tasks that aren't in stopped state");
				}
			}
			else
			{
				cemuLog_logDebug(LogType::Force, "IOSU-Boss: Unsupported StartScheduling called for task {}", taskId.id.c_str());
			}
		}

		void StopScheduleTask(uint32 persistentId, const TaskId& taskId)
		{
			// todo
			// task scheduling is not yet implemented so this does nothing for now
		}

		std::shared_ptr<RegisteredTask> GetRegisteredTask2(const uint32 persistentId, const TaskId& taskId)
		{
			std::unique_lock _l(m_taskMtx);
			auto it = m_registeredTasks.find(taskId); // todo - check persistentId as well (make it part of the key?)
			if (it != m_registeredTasks.end())
				return it->second;
			return nullptr;
		}

	private:
		void BossDaemonThread()
		{
			CURL* curl = curl_easy_init();
			while ( m_threadRunning )
			{
				// check for tasks to run
				{
					std::shared_ptr<RegisteredTask> task = GetNextRunableTask();
					if (task)
					{
						task->TaskDoRequest(curl);
						cemu_assert_debug(task->GetState() != TaskState::Ready);
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			curl_easy_cleanup(curl);
		}

		std::shared_ptr<RegisteredTask> GetNextRunableTask()
		{
			std::unique_lock _l(m_taskMtx);
			for (auto& task : m_registeredTasks)
			{
				if (task.second->GetState() == TaskState::Ready)
					return task.second;
			}
			return nullptr;
		}

		std::thread m_bossDaemonThread;
		std::atomic_bool m_threadRunning{ false };
		// task list
		std::mutex m_taskMtx;
		std::map<TaskId, std::shared_ptr<RegisteredTask>> m_registeredTasks;
	}s_bossDaemon;

	class BossMainService : public iosu::nn::IPCService
	{
	public:
		BossMainService() : iosu::nn::IPCService("/dev/boss") {}

		nnResult ServiceCall(IPCServiceCall& serviceCall) override
		{
			if (serviceCall.GetServiceId() == 0)
			{
				switch (static_cast<BossCommandId>(serviceCall.GetCommandId()))
				{
				case BossCommandId::TaskIsRegistered:
					return HandleTaskIsRegistered(serviceCall);
				case BossCommandId::TaskRegisterA:
					return HandleTaskRegister(serviceCall, false);
				case BossCommandId::TaskRegisterForImmediateRunA:
					return HandleTaskRegister(serviceCall, true);
				case BossCommandId::TaskUnregister:
					return HandleTaskUnregister(serviceCall);
				case BossCommandId::TaskRun:
					return HandleTaskRun(serviceCall);
				case BossCommandId::TaskStartScheduling:
					return HandleTaskStartScheduling(serviceCall);
				case BossCommandId::TaskStopScheduling:
					return HandleTaskStopScheduling(serviceCall);
				case BossCommandId::TaskWaitA:
					return HandleTaskWait(serviceCall);
				case BossCommandId::TaskGetTurnState:
					return HandleTaskGetTurnState(serviceCall);
				case BossCommandId::TaskGetHttpStatusCodeA:
					return HandleTaskGetHttpStatusCodeA(serviceCall);
				case BossCommandId::TaskGetContentLength:
					return HandleTaskGetContentLength(serviceCall);
				case BossCommandId::TaskGetProcessedLength:
					return HandleTaskGetProcessedLength(serviceCall);
				case BossCommandId::UknA7:
					return HandleUknA7(serviceCall);
				case BossCommandId::DeleteDataRelated:
					return HandleDeleteDataRelated(serviceCall);
				case BossCommandId::StorageExist:
					return HandleStorageExist(serviceCall);
				case BossCommandId::StorageGetDataList:
					return HandleStorageGetDataList(serviceCall);
				case BossCommandId::NsDataExist:
					return HandleNsDataExist(serviceCall);
				case BossCommandId::NsDataRead:
					return HandleNsDataRead(serviceCall);
				case BossCommandId::NsDataGetSize:
					return HandleNsDataGetSize(serviceCall);
				case BossCommandId::NsDataDeleteFile:
					return HandleNsDataDeleteFile(serviceCall, false);
				case BossCommandId::NsDataDeleteFileWithHistory:
					return HandleNsDataDeleteFile(serviceCall, true);
				case BossCommandId::NsFinalize:
					return HandleNsFinalize(serviceCall);
				case BossCommandId::TitleSetNewArrivalFlag:
					return HandleTitleSetNewArrivalFlag(serviceCall);
				default:
					cemuLog_log(LogType::Force, "Unsupported service call to /dev/boss");
					cemu_assert_unimplemented();
					break;
				}
			}
			else
			{
				cemu_assert_suspicious();
			}
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0);
		}

		// helper function to get proper persistentId
		uint32 BossGetPersistentId(uint32 persistentId)
		{
			if (persistentId != 0)
				return persistentId;
			uint32 currentPersistentId;
			bool r = iosu::act::GetPersistentId(iosu::act::ACT_SLOT_CURRENT, &currentPersistentId);;
			if (!r)
				return 0;
			return currentPersistentId;
		}

		nnResult HandleTaskIsRegistered(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			bool isRegistered = s_bossDaemon.TaskIsRegistered(persistentId, taskId);
			// write response
			serviceCall.WriteResponse<uint8be>(isRegistered ? 1 : 0);
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskRegister(IPCServiceCall& serviceCall, bool forImmediateRun)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			IPCServiceCall::UnalignedBuffer taskSettingsBuffer = serviceCall.ReadUnalignedInputBufferInfo();
			TaskSettingCore taskSetting = taskSettingsBuffer.ReadType<TaskSettingCore>(); // bugged.. (but on the submission side)
			if (taskSetting.persistentId == 0)
				taskSetting.persistentId = persistentId;
			taskSetting.taskId = taskId;
			s_bossDaemon.RegisterTask(taskSetting);
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskUnregister(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			s_bossDaemon.UnregisterTask(persistentId, taskId);
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskRun(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			bool isForegroundRun = serviceCall.ReadParameter<uint8be>() != 0; // todo - handle this
			s_bossDaemon.TaskRun(persistentId, taskId);
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskStartScheduling(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			uint8 startImmediately = serviceCall.ReadParameter<uint8be>();
			s_bossDaemon.StartScheduleTask(persistentId, taskId, startImmediately != 0);
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskStopScheduling(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			s_bossDaemon.StopScheduleTask(persistentId, taskId);
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskWait(IPCServiceCall& serviceCall)
		{
			static_assert(sizeof(betype<TaskWaitState>) == 4);
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			TaskWaitState waitState = serviceCall.ReadParameter<betype<TaskWaitState>>();
			uint32 timeout = serviceCall.ReadParameter<uint32be>();
			std::chrono::steady_clock::time_point endTime;
			if (timeout != 0)
				endTime = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
			std::shared_ptr<RegisteredTask> registeredTask = s_bossDaemon.GetRegisteredTask2(persistentId, taskId);
			cemu_assert_debug(registeredTask != nullptr);
			if (!registeredTask)
				return RESULT_NOTEXIST;
			while (true)
			{
				// todo - make async
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				TaskState currentState = registeredTask->GetState();
				// does the state match the wait state?
				if (waitState == TaskWaitState::Done)
				{
					if (currentState == TaskState::Done)
					{
						serviceCall.WriteResponse<uint8be>(1); // 0 -> timeout, 1 -> no timeout
						return RESULT_SUCCESS;
					}
				}
				else
				{
					cemuLog_log(LogType::Force, "IOSU-Boss: Unknown task wait state {}", static_cast<uint32>(waitState));
					serviceCall.WriteResponse<uint8be>(0);
					return RESULT_SUCCESS;
				}
				// check for timeout
				if (timeout != 0 && std::chrono::steady_clock::now() >= endTime)
				{
					serviceCall.WriteResponse<uint8be>(0);
					return RESULT_SUCCESS; // which error to return here?
				}
			}
		}

		nnResult HandleTaskGetTurnState(IPCServiceCall& serviceCall)
		{
			static_assert(sizeof(betype<TaskTurnState>) == 4);
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			std::shared_ptr<RegisteredTask> registeredTask = s_bossDaemon.GetRegisteredTask2(persistentId, taskId);
			TaskTurnState turnState = registeredTask ? registeredTask->GetTurnState() : TaskTurnState::Ukn;
			serviceCall.WriteResponse<uint32be>(1); // execution counter - todo
			serviceCall.WriteResponse<betype<TaskTurnState>>(turnState);
			if (!registeredTask)
				return RESULT_NOTEXIST;
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskGetHttpStatusCodeA(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			std::shared_ptr<RegisteredTask> registeredTask = s_bossDaemon.GetRegisteredTask2(persistentId, taskId);
			sint32 httpStatusCode = registeredTask ? registeredTask->GetHttpStatusCode() : 0;
			serviceCall.WriteResponse<sint32be>(1); // execution counter - todo
			serviceCall.WriteResponse<sint32be>(httpStatusCode);
			if (!registeredTask)
				return RESULT_NOTEXIST;
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskGetContentLength(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			std::shared_ptr<RegisteredTask> registeredTask = s_bossDaemon.GetRegisteredTask2(persistentId, taskId);
			uint32 contentLength = registeredTask ? registeredTask->GetContentLength() : 0;
			serviceCall.WriteResponse<uint32be>(1); // execution counter - todo
			serviceCall.WriteResponse<uint64be>(contentLength);
			if (!registeredTask)
				return RESULT_NOTEXIST;
			return RESULT_SUCCESS;
		}

		nnResult HandleTaskGetProcessedLength(IPCServiceCall& serviceCall)
		{
			uint32 persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			TaskId taskId = serviceCall.ReadParameter<TaskId>();
			std::shared_ptr<RegisteredTask> registeredTask = s_bossDaemon.GetRegisteredTask2(persistentId, taskId);
			uint32 processedLength = registeredTask ? registeredTask->GetProcessedLength() : 0;
			serviceCall.WriteResponse<uint32be>(1); // execution counter - todo
			serviceCall.WriteResponse<uint64be>(processedLength); // this the actual length?
			if (!registeredTask)
				return RESULT_NOTEXIST;
			return RESULT_SUCCESS;
		}

		/* Storage */

		nnResult HandleStorageExist(IPCServiceCall& serviceCall)
		{
			static_assert(sizeof(DirectoryName) == 8);
			static_assert(sizeof(betype<StorageKind>) == 4);
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			DirectoryName directoryName = serviceCall.ReadParameter<DirectoryName>();
			StorageKind storageKind = serviceCall.ReadParameter<betype<StorageKind>>();
			if (!persistentId)
				return RESULT_C0203880;
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			// todo - investigate how IOSU handles this
			bool storageExists = true;
			if (storageKind == StorageKind::StorageNbdl)
			{
				// but for now we only check if the fad.db file exists for nbdl
				storageExists = m_fadDb.CheckIfStorageExists(directoryName, titleId, persistentId);
			}
			else
			{
				cemuLog_logDebug(LogType::Force, "IOSU-Boss: Unhandled storage kind {}", static_cast<uint32>(storageKind));
				cemu_assert_unimplemented();
			}
			serviceCall.WriteResponse<uint8be>(storageExists ? 1 : 0);
			return RESULT_SUCCESS;
		}

		nnResult HandleStorageGetDataList(IPCServiceCall& serviceCall)
		{
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			DirectoryName directoryName = serviceCall.ReadParameter<DirectoryName>();
			uint32 ukn = serviceCall.ReadParameter<uint32be>();
			cemu_assert_debug(ukn == 0);
			IPCServiceCall::UnalignedBuffer dataListOutputBuffer = serviceCall.ReadUnalignedOutputBufferInfo();
			uint32 startOffset = serviceCall.ReadParameter<uint32be>(); // guessed
			cemu_assert_debug(startOffset == 0); // todo - implement offset
			size_t dataListSize = dataListOutputBuffer.GetSize() / sizeof(DataName);
			std::vector<DataName> dataListTmp;
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			// open FAD database
			TaskId taskId(directoryName.name2.c_str());
			nnResult r = m_fadDb.Load(taskId, titleId, persistentId);
			if (NN_RESULT_IS_FAILURE(r))
				return r;
			m_fadDb.GetDataList(dataListTmp);
			if (dataListTmp.size() > dataListSize)
				dataListTmp.resize(dataListSize);
			// write to output
			dataListOutputBuffer.WriteData(dataListTmp.data(), dataListTmp.size() * sizeof(DataName));
			serviceCall.WriteResponse<uint32be>((uint32)dataListTmp.size());
			serviceCall.WriteResponse<uint8be>(1); // extra byte to indicate success or failure
			return RESULT_SUCCESS;
		}

		/* NsData */

		nnResult HandleNsDataExist(IPCServiceCall& serviceCall)
		{
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			DirectoryName directoryName = serviceCall.ReadParameter<DirectoryName>();
			uint32 ukn = serviceCall.ReadParameter<uint32be>(); // storage kind?
			DataName fileName = serviceCall.ReadParameter<DataName>();
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			nnResult r = m_nsDataAccessor.Open(directoryName, titleId, persistentId, fileName);
			if (NN_RESULT_IS_FAILURE(r))
				return r;
			bool nsDataExists = m_nsDataAccessor.Exists();
			serviceCall.WriteResponse<uint8be>(nsDataExists);
			return RESULT_SUCCESS;
		}

		nnResult HandleNsDataRead(IPCServiceCall& serviceCall)
		{
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			DirectoryName directoryName = serviceCall.ReadParameter<DirectoryName>();
			uint32 ukn = serviceCall.ReadParameter<uint32be>(); // storage kind?
			DataName fileName = serviceCall.ReadParameter<DataName>();
			IPCServiceCall::UnalignedBuffer dataOutputBuffer = serviceCall.ReadUnalignedOutputBufferInfo();
			uint64 readOffset = serviceCall.ReadParameter<uint64be>();
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			nnResult r = m_nsDataAccessor.Open(directoryName, titleId, persistentId, fileName);
			if (NN_RESULT_IS_FAILURE(r))
			{
				serviceCall.WriteResponse<uint64be>(0); // should we still write the bytes read in case of failure?
				return r;
			}
			uint32 bytesRead = 0;
			std::vector<uint8> tmpBuffer;
			tmpBuffer.resize(dataOutputBuffer.GetSize());
			bool rRead = m_nsDataAccessor.Read(tmpBuffer.data(), tmpBuffer.size(), readOffset, bytesRead);
			dataOutputBuffer.WriteData(tmpBuffer.data(), tmpBuffer.size());
			serviceCall.WriteResponse<uint64be>(bytesRead);
			// todo - handle rRead
			return RESULT_SUCCESS;
		}

		nnResult HandleNsDataGetSize(IPCServiceCall& serviceCall)
		{
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			DirectoryName directoryName = serviceCall.ReadParameter<DirectoryName>();
			uint32 ukn = serviceCall.ReadParameter<uint32be>(); // storage kind?
			DataName fileName = serviceCall.ReadParameter<DataName>();
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			nnResult r = m_nsDataAccessor.Open(directoryName, titleId, persistentId, fileName);
			if (NN_RESULT_IS_FAILURE(r))
				return r;
			uint32 fileSize = 0;
			r = m_nsDataAccessor.GetSize(fileSize);
			serviceCall.WriteResponse<uint64be>(fileSize);
			return r;
		}

		nnResult HandleNsDataDeleteFile(IPCServiceCall& serviceCall, bool withHistory)
		{
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			DirectoryName directoryName = serviceCall.ReadParameter<DirectoryName>();
			uint32 ukn = serviceCall.ReadParameter<uint32be>(); // storage kind?
			DataName fileName = serviceCall.ReadParameter<DataName>();
			uint64 titleId = CafeSystem::GetForegroundTitleId();
			nnResult r = m_nsDataAccessor.Open(directoryName, titleId, persistentId, fileName);
			if (NN_RESULT_IS_FAILURE(r))
				return r;
			// todo - handle withHistory
			return m_nsDataAccessor.Delete();
		}

		nnResult HandleNsFinalize(IPCServiceCall& serviceCall)
		{
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			DirectoryName directoryName = serviceCall.ReadParameter<DirectoryName>();
			uint32 ukn = serviceCall.ReadParameter<uint32be>(); // storage kind?
			DataName fileName = serviceCall.ReadParameter<DataName>();
			m_nsDataAccessor.Close();
			return RESULT_SUCCESS;
		}

		/* Title */

		nnResult HandleTitleSetNewArrivalFlag(IPCServiceCall& serviceCall)
		{
			auto persistentId = BossGetPersistentId(serviceCall.ReadParameter<uint32be>());
			if (!persistentId)
				return RESULT_C0203880;
			uint8 flagValue = serviceCall.ReadParameter<uint8be>();
			cemuLog_logDebug(LogType::Force, "IOSU-Boss: Unhandled command TitleSetNewArrivalFlag");
			return RESULT_SUCCESS;
		}

		/* stubbed opcodes */
		nnResult HandleUknA7(IPCServiceCall& serviceCall)
		{
			cemuLog_logDebug(LogType::Force, "IOSU-Boss: Unhandled command A7");
			// no response data
			return RESULT_SUCCESS;
		}

		nnResult HandleDeleteDataRelated(IPCServiceCall& serviceCall)
		{
			cemuLog_logDebug(LogType::Force, "IOSU-Boss: Unhandled delete command");
			// no response data
			return RESULT_SUCCESS;
		}


	};

	BossMainService s_bossService;

	class : public ::IOSUModule
	{
		void TitleStart() override
		{
			s_bossService.Start();
			s_bossDaemon.Start();
		}
		void TitleStop() override
		{
			s_bossService.Stop();
			m_fadDb.Clear();
			m_nsDataAccessor.Close();
		}
	}sIOSUModuleNNBOSS;

	IOSUModule* GetModule()
	{
		return static_cast<IOSUModule*>(&sIOSUModuleNNBOSS);
	}
}
