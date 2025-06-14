#pragma once
#include "Common/CafeString.h"

namespace nn::boss
{
	typedef uint32 BossResult;

	struct VTableEntry
	{
		uint16be offsetA{0};
		uint16be offsetB{0};
		MEMPTR<void> ptr;
	};
	static_assert(sizeof(VTableEntry) == 8);

	struct TitleId
	{
		uint64be u64{};

		static bool IsValid(TitleId* _thisptr);
		static TitleId* ctorDefault(TitleId* _thisptr);
		static TitleId* ctorFromTitleId(TitleId* _thisptr, uint64 titleId); // __ct__Q3_2nn4boss7TitleIDFUL
		static TitleId* ctorCopy(TitleId* _thisptr, TitleId* titleId); // __ct__Q3_2nn4boss7TitleIDFRCQ3_2nn4boss7TitleID
		static bool operator_ne(TitleId* _thisptr, TitleId* titleId);

	};
	static_assert(sizeof(TitleId) == 8);

	struct TaskId
	{
		CafeString<8> id;

		TaskId() = default;
		TaskId(const char* taskId) { id.assign(taskId); }

		static TaskId* ctorDefault(TaskId* _thisptr);
		static TaskId* ctorFromString(TaskId* _thisptr, const char* taskId);

		//auto operator<=>(const TaskId& other) const = default;
		std::strong_ordering operator<=>(const TaskId& other) const noexcept {
			return id <=> other.id; // Delegate to CafeString's operator<=>
		}
	};
	static_assert(sizeof(TaskId) == 8);

	struct Title
	{
		uint32be accountId{}; // 0x00
		TitleId titleId{}; // 0x8
		MEMPTR<void> vTablePtr{}; // 0x10

		struct VTable
		{
			VTableEntry rtti;
			VTableEntry dtor;
		};
		static inline SysAllocator<VTable> s_titleVTable;

		static Title* ctor(Title* _this);
		static void dtor(Title* _this, uint32 options);
		static void InitVTable();
	};
	static_assert(sizeof(Title) == 0x18);

	struct DirectoryName
	{
		CafeString<8> name2;

		static DirectoryName* ctor(DirectoryName* _thisptr);
		static const char* operator_const_char(DirectoryName* _thisptr);
	};
	static_assert(sizeof(DirectoryName) == 8);

	struct BossAccount // the actual class name is "Account" and while the boss namespace helps us separate this from Account(.h) we use an alternative name to avoid confusion
	{
		struct VTable
		{
			VTableEntry rtti;
			VTableEntry dtor;
		};
		static inline SysAllocator<VTable> s_VTable;

		uint32be accountId;
		MEMPTR<void> vTablePtr;

		static BossAccount* ctor(BossAccount* _this, uint32 accountId);
		static void dtor(BossAccount* _this, uint32 options);
		static void InitVTable();

	};
	static_assert(sizeof(BossAccount) == 8);

	enum class TaskWaitState : uint32 // for Task::Wait()
	{
		Done = 1,
	};

	enum class TaskTurnState : uint32
	{
		Ukn = 0,
		Stopped = 1,
		Ready = 4,
		Running = 6,
		Done = 7, // how does this differ from DoneSuccess?
		DoneSuccess = 16,
		DoneError = 17
	};

	enum class TaskState : uint8
	{
		Initial = 0,
		Stopped = 1,
		Ready = 4, // waiting for turn to run
		Running = 6,
		Done = 7,
	};

	enum class TaskType : uint16
	{
		NbdlTaskSetting = 2,
		RawDlTaskSetting_1 = 1,
		RawDlTaskSetting_3 = 3,
		RawDlTaskSetting_9 = 9,
		RawUlTaskSetting = 4,
		PlayLogUploadTaskSetting = 5,
		PlayReportSetting = 6,
		DataStoreDownloadSetting = 8,
		NbdlDataListTaskSetting = 10
	};

	struct TaskSettingCore // the setting struct as used by IOSU
	{
		struct LifeTime
		{
			uint32be high;
			uint32be low;
		};

		uint32be persistentId;
		uint32be ukn04;
		uint32be ukn08;
		uint32be ukn0C;
		uint32be ukn10;
		uint32be ukn14;
		uint32be ukn18;
		uint32be ukn1C;
		TaskId taskId; // +0x20
		betype<TaskType> taskType; // +0x28
		uint8be priority; // +0x2A
		uint8be mode; // +0x2B
		uint8be permission; // +0x2C
		uint16be intervalA; // +0x2E
		uint32be intervalB; // +0x30
		uint32be unk34; // +0x34 - could be padding
		LifeTime lifeTime; // +0x38 - this is a 64bit value, but the whole struct has a size of 0x1004 and doesnt preserve the alignment in an array. Its probably handled as two 32bit values?
		uint8be httpProtocol; // +0x40
		uint8be internalClientCert; // +0x41
		uint16be httpOption; // +0x42
		uint8 ukn44[0x48 - 0x44]; // padding?
		CafeString<256> url; // +0x48
		CafeString<64> lastModifiedTime; // +0x148
		uint8be internalCaCert[3]; // +0x188
		uint8 ukn18B; // +0x18B - padding?
		uint32be httpTimeout; // +0x18C
		CafeString<128> caCert[3]; // +0x190 - 3 entries, each 0x80 bytes
		CafeString<128> clientCertName; // +0x310
		CafeString<128> clientCertKey; // +0x390
		struct HttpHeader
		{
			CafeString<32> name; // +0x00
			CafeString<64> value; // +0x20
		};
		HttpHeader httpHeaders[3]; // +0x410
		CafeString<96> httpQueryString; // +0x530
		CafeString<512> serviceToken; // +0x590

		uint8 ukn790[0x7C0 - 0x790];
		// after 0x7C0 the task-specific fields seem to start?

		union
		{
			struct
			{
				uint32be optionValue; // +0x7C0
				CafeString<32> largeHttpHeaderKey; // +0x7C4
				CafeString<512> largeHttpHeaderValue; // +0x7E4
			}rawUl; // RawUlTaskSetting
			struct
			{
				uint32be optionValue; // +0x7C0
				CafeString<32> largeHttpHeaderKey; // +0x7C4
				CafeString<512> largeHttpHeaderValue; // +0x7E4
				// inherits the above values from RawUlTaskSetting
				// the play report fields are too large to fit into the available space, so instead they are passed via their own system call that attaches it to the task. See PlayReportSetting::RegisterPreprocess
			}playReportSetting;
			struct
			{
				uint8be newArrival; // +0x7C0
				uint8be led; // +0x7C1
				uint8be ukn7C2[6]; // +0x7C2 - padding?
				CafeString<8> bossDirectory; // +0x7C8
				CafeString<32> fileName; // +0x7D0 Not 100% sure this is fileName
			}rawDl; // RawDlTaskSetting
			struct
			{
				CafeString<32> bossCode; // +0x7C0
				CafeString<8> bossDirectory; // +0x7E0
				uint32be ukn7E8;
				uint32be ukn7EC;
				uint32be directorySizeLimitHigh; // +0x7F0
				uint32be directorySizeLimitLow; // +0x7F4
				CafeString<32> fileName; // +0x7F8
				// more fields here...
			}nbdl;
			struct
			{
				uint8 finalPadding[0xC00 - 0x7C0];
			}paddedBlock;
		};
	};
	static_assert(sizeof(TaskSettingCore) == 0xC00);

	struct TaskSetting : public TaskSettingCore
	{
		uint8 paddingC00[0x1000 - 0xC00];
		MEMPTR<void> vTablePtr; // +0x1000

		struct VTableTaskSetting
		{
			VTableEntry rtti;
			VTableEntry dtor;
			VTableEntry RegisterPreprocess; // todo - double check the offset
			VTableEntry unk1;
		};
		static inline SysAllocator<VTableTaskSetting> s_VTable;

		static TaskSetting* ctor(TaskSetting* _thisptr);
		static void dtor(TaskSetting* _this, uint32 options);
		static bool IsPrivileged(TaskSetting* _thisptr);
		static void InitializeSetting(TaskSetting* _thisptr);
		static void InitVTable();
	};
	static_assert(offsetof(TaskSetting, priority) == 0x2A);
	static_assert(offsetof(TaskSetting, mode) == 0x2B);
	static_assert(offsetof(TaskSetting, permission) == 0x2C);
	static_assert(offsetof(TaskSetting, intervalA) == 0x2E);
	static_assert(offsetof(TaskSetting, intervalB) == 0x30);
	static_assert(offsetof(TaskSetting, lifeTime) == 0x38);
	static_assert(offsetof(TaskSetting, httpProtocol) == 0x40);
	static_assert(offsetof(TaskSetting, internalClientCert) == 0x41);
	static_assert(offsetof(TaskSetting, httpOption) == 0x42);
	static_assert(offsetof(TaskSetting, url) == 0x48);
	static_assert(offsetof(TaskSetting, lastModifiedTime) == 0x148);
	static_assert(offsetof(TaskSetting, internalCaCert) == 0x188);
	static_assert(offsetof(TaskSetting, httpTimeout) == 0x18C);
	static_assert(offsetof(TaskSetting, caCert) == 0x190);
	static_assert(offsetof(TaskSetting, clientCertName) == 0x310);
	static_assert(offsetof(TaskSetting, clientCertKey) == 0x390);
	static_assert(offsetof(TaskSetting, httpHeaders) == 0x410);
	static_assert(offsetof(TaskSetting, httpQueryString) == 0x530);
	static_assert(offsetof(TaskSetting, serviceToken) == 0x590);
	// rawUl
	static_assert(offsetof(TaskSetting, rawUl.optionValue) == 0x7C0);
	static_assert(offsetof(TaskSetting, rawUl.largeHttpHeaderKey) == 0x7C4);
	static_assert(offsetof(TaskSetting, rawUl.largeHttpHeaderValue) == 0x7E4);
	// rawDl
	static_assert(offsetof(TaskSetting, rawDl.newArrival) == 0x7C0);
	static_assert(offsetof(TaskSetting, rawDl.bossDirectory) == 0x7C8);
	static_assert(offsetof(TaskSetting, rawDl.fileName) == 0x7D0);
	// nbdl
	static_assert(offsetof(TaskSetting, nbdl.bossCode) == 0x7C0);
	static_assert(offsetof(TaskSetting, nbdl.bossDirectory) == 0x7E0);

	static_assert(offsetof(TaskSetting, vTablePtr) == 0x1000);
	static_assert(sizeof(TaskSetting) == 0x1004);

	/* NetTaskSetting */

	struct NetTaskSetting : TaskSetting
	{
		struct VTableNetTaskSetting : public VTableTaskSetting
		{ };
		static inline SysAllocator<VTableNetTaskSetting> s_VTable;

		static NetTaskSetting* ctor(NetTaskSetting* _thisptr);
		static BossResult AddCaCert(NetTaskSetting* _thisptr, const char* name);
		static BossResult SetServiceToken(NetTaskSetting* _thisptr, const uint8* serviceToken);
		static BossResult AddInternalCaCert(NetTaskSetting* _thisptr, char certId);
		static void SetInternalClientCert(NetTaskSetting* _thisptr, char certId);
		static void InitVTable();
	};
	static_assert(sizeof(NetTaskSetting) == 0x1004);

	/* NbdlTaskSetting */

	struct NbdlTaskSetting : NetTaskSetting
	{
		struct VTableNbdlTaskSetting : public VTableNetTaskSetting
		{
			VTableEntry rttiNetTaskSetting; // unknown
		};
		static_assert(sizeof(VTableNbdlTaskSetting) == 8*5);
		static inline SysAllocator<VTableNbdlTaskSetting> s_VTable;

		static NbdlTaskSetting* ctor(NbdlTaskSetting* _thisptr);
		static BossResult Initialize(NbdlTaskSetting* _thisptr, const char* bossCode, uint64 directorySizeLimit, const char* bossDirectory);
		static BossResult SetFileName(NbdlTaskSetting* _thisptr, const char* fileName);
		static void InitVTable();
	};
	static_assert(sizeof(NbdlTaskSetting) == 0x1004);

	/* RawUlTaskSetting */

	struct RawUlTaskSetting : NetTaskSetting
	{
		uint32be ukRaw1; // 0x1004
		uint32be ukRaw2; // 0x1008
		uint32be ukRaw3; // 0x100C
		uint8 rawSpace[0x200]; // 0x1010

		struct VTableRawUlTaskSetting : public VTableNetTaskSetting
		{
			VTableEntry rttiNetTaskSetting; // unknown
		};
		static_assert(sizeof(VTableRawUlTaskSetting) == 8*5);
		static inline SysAllocator<VTableRawUlTaskSetting> s_VTable;

		static RawUlTaskSetting* ctor(RawUlTaskSetting* _thisptr);
		static void dtor(RawUlTaskSetting* _this, uint32 options);
		static void InitVTable();
	};
	static_assert(sizeof(RawUlTaskSetting) == 0x1210);

	/* RawDlTaskSetting */
	struct RawDlTaskSetting : NetTaskSetting
	{
		struct VTableRawDlTaskSetting : public VTableNetTaskSetting
		{
			VTableEntry rttiNetTaskSetting; // unknown
		};
		static_assert(sizeof(VTableRawDlTaskSetting) == 8*5);
		static inline SysAllocator<VTableRawDlTaskSetting> s_VTable;

		static RawDlTaskSetting* ctor(RawDlTaskSetting* _thisptr);
		static BossResult Initialize(RawDlTaskSetting* _thisptr, const char* url, bool newArrival, bool led, const char* fileName, const char* bossDirectory);

		static void InitVTable();
	};
	static_assert(sizeof(RawDlTaskSetting) == 0x1004);

	/* PlayReportSetting */

	struct PlayReportSetting : RawUlTaskSetting
	{
		MEMPTR<uint8> ukn1210_ptr; // 0x1210
		uint32be ukn1214_size; // 0x1214
		uint32be ukPlay3; // 0x1218
		uint32be ukPlay4; // 0x121C

		struct VTablePlayReportSetting : public VTableRawUlTaskSetting
		{};
		static_assert(sizeof(VTablePlayReportSetting) == 8*5);
		static inline SysAllocator<VTablePlayReportSetting> s_VTable;

		static PlayReportSetting* ctor(PlayReportSetting* _this);
		static void dtor(PlayReportSetting* _this, uint32 options);
		static void Initialize(PlayReportSetting* _this, uint8* ptr, uint32 size);
		static bool Set(PlayReportSetting* _this, const char* keyname, uint32 value);
		static void InitVTable();
	};
	static_assert(sizeof(PlayReportSetting) == 0x1220);

	/* Storage */
	enum class StorageKind : uint32
	{
		StorageNbdl = 0,
		StorageRawDl = 1,
	};

	struct DataName
	{
		CafeString<32> name;

		static DataName* ctor(DataName* _this); // __ct__Q3_2nn4boss8DataNameFv
		static const char* operator_const_char(DataName* _this); // __opPCc__Q3_2nn4boss8DataNameCFv

		const char* c_str() const
		{
			return name.c_str();
		}
	};
	static_assert(sizeof(DataName) == 32);

	/* IPC commands for /dev/boss */
	enum class BossCommandId : uint32
	{
		// task operations
		TaskIsRegistered = 0x69,
		TaskRegisterA = 0x6A,
		TaskRegisterForImmediateRunA = 0x6B,
		TaskUnregister = 0x6D,
		TaskRun = 0x78,
		TaskStartScheduling = 0x77,
		TaskStopScheduling = 0x79,
		TaskWaitA = 0x7A,
		TaskGetHttpStatusCodeA = 0x7C,
		TaskGetTurnState = 0x7E,
		TaskGetContentLength = 0x82,
		TaskGetProcessedLength = 0x83,
		// storage operations
		StorageExist = 0x87,
		StorageGetDataList = 0xB0,
		// NsData operations
		NsDataExist = 0x90,
		NsDataRead = 0x93,
		NsDataGetSize = 0x96,
		NsDataDeleteFile = 0xA7,
		NsDataDeleteFileWithHistory = 0xA8,
		NsFinalize = 0xB3,
		// Title operations
		TitleSetNewArrivalFlag = 0x9E,

		// most (if not all?) opcodes seem to have a secondary form with an additional titleId parameter in the high range around 0x150 and serviceId 1

		// unknown commands
		UknA7 = 0xA5,
		DeleteDataRelated = 0xA6,
	};


}