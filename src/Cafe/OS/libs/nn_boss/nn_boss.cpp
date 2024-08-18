#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/IOSU/legacy/iosu_boss.h"
#include "Cafe/IOSU/legacy/iosu_act.h"
#include "config/ActiveSettings.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/Filesystem/fsc.h"

namespace nn
{
	typedef uint32 Result;
namespace boss
{
#define bossPrepareRequest() \
StackAllocator<iosuBossCemuRequest_t> _buf_bossRequest; \
StackAllocator<ioBufferVector_t> _buf_bufferVector; \
iosuBossCemuRequest_t* bossRequest = _buf_bossRequest.GetPointer(); \
ioBufferVector_t* bossBufferVector = _buf_bufferVector.GetPointer(); \
memset(bossRequest, 0, sizeof(iosuBossCemuRequest_t)); \
memset(bossBufferVector, 0, sizeof(ioBufferVector_t)); \
bossBufferVector->buffer = (uint8*)bossRequest;

	SysAllocator<coreinit::OSMutex> g_mutex;
	sint32 g_initCounter = 0;
	bool g_isInitialized = false;

	struct VTableEntry
	{
		uint16be offsetA{0};
		uint16be offsetB{0};
		MEMPTR<void> ptr;
	};
	static_assert(sizeof(VTableEntry) == 8);

	#define DTOR_WRAPPER(__TYPE) RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) { dtor(MEMPTR<__TYPE>(hCPU->gpr[3]), hCPU->gpr[4]); osLib_returnFromFunction(hCPU, 0); })

	constexpr uint32 BOSS_MEM_MAGIC = 0xCAFE4321;

	template<typename T>
	MEMPTR<T> boss_new()
	{
		uint32 objSize = sizeof(T);
		uint32be* basePtr = (uint32be*)coreinit::_weak_MEMAllocFromDefaultHeapEx(objSize + 8, 0x8);
		basePtr[0] = BOSS_MEM_MAGIC;
		basePtr[1] = objSize;
		return (T*)(basePtr+2);
	}

	void boss_delete(MEMPTR<void> mem)
	{
		if(!mem)
			return;
		uint32be* basePtr = (uint32be*)mem.GetPtr() - 2;
		if(basePtr[0] != BOSS_MEM_MAGIC)
		{
			cemuLog_log(LogType::Force, "nn_boss: Detected memory corruption");
			cemu_assert_suspicious();
		}
		coreinit::_weak_MEMFreeToDefaultHeap(basePtr);
	}

	Result Initialize() // Initialize__Q2_2nn4bossFv
	{
		coreinit::OSLockMutex(&g_mutex);
		Result result = 0;
		if(g_initCounter == 0)
		{
			g_isInitialized = true;
			// IPC init here etc.
			result = 0x200080; // init result
		}
		g_initCounter++;
		coreinit::OSUnlockMutex(&g_mutex);
		return NN_RESULT_IS_SUCCESS(result) ? 0 : result;
	}

	uint32 IsInitialized() // IsInitialized__Q2_2nn4bossFv
	{
		return g_isInitialized;
	}

	void Finalize() // Finalize__Q2_2nn4bossFv
	{
		coreinit::OSLockMutex(&g_mutex);
		if(g_initCounter == 0)
			cemuLog_log(LogType::Force, "nn_boss: Finalize() called without corresponding Initialize()");
		if(g_initCounter == 1)
		{
			g_isInitialized = false;
			// IPC deinit here etc.
		}
		g_initCounter--;
		coreinit::OSUnlockMutex(&g_mutex);
	}

	uint32 GetBossState(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "nn_boss.GetBossState() - stub");
		return 7;
	}

	struct TitleId
	{
		uint64be u64{};

		static TitleId* ctor(TitleId* _thisptr, uint64 titleId)
		{
			if (!_thisptr)
				_thisptr = boss_new<TitleId>();
			_thisptr->u64 = titleId;
			return _thisptr;
		}

		static TitleId* ctor(TitleId* _thisptr)
		{
			return ctor(_thisptr, 0);
		}

		static bool IsValid(TitleId* _thisptr)
		{
			return _thisptr->u64 != 0;
		}

		static TitleId* ctor1(TitleId* _thisptr, uint32 filler, uint64 titleId)
		{
			return ctor(_thisptr);
		}

		static TitleId* ctor2(TitleId* _thisptr, uint32 filler, uint64 titleId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_TitleId_ctor2(0x{:x})", MEMPTR(_thisptr).GetMPTR());
			if (!_thisptr)
			{
				// _thisptr = new Task_t
				assert_dbg();
			}

			_thisptr->u64 = titleId;
			return _thisptr;
		}

		static TitleId* ctor3(TitleId* _thisptr, TitleId* titleId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_TitleId_cctor(0x{:x})", MEMPTR(_thisptr).GetMPTR());
			if (!_thisptr)
				_thisptr = boss_new<TitleId>();
			_thisptr->u64 = titleId->u64;
			return _thisptr;
		}

		static bool operator_ne(TitleId* _thisptr, TitleId* titleId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_TitleId_operator_ne(0x{:x})", MEMPTR(_thisptr).GetMPTR());
			return _thisptr->u64 != titleId->u64;
		}
	};
	static_assert(sizeof(TitleId) == 8);

	struct TaskId
	{
		char id[0x8]{};

		static TaskId* ctor(TaskId* _thisptr)
		{
			if(!_thisptr)
				_thisptr = boss_new<TaskId>();
			_thisptr->id[0] = '\0';
			return _thisptr;
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

		static Title* ctor(Title* _this)
		{
			if (!_this)
				_this = boss_new<Title>();
			*_this = {};
			_this->vTablePtr = s_titleVTable;
			return _this;
		}

		static void dtor(Title* _this, uint32 options)
		{
			if (_this && (options & 1))
				boss_delete(_this);
		}

		static void InitVTable()
		{
			s_titleVTable->rtti.ptr = nullptr; // todo
			s_titleVTable->dtor.ptr = DTOR_WRAPPER(Title);
		}
	};
	static_assert(sizeof(Title) == 0x18);

	struct DirectoryName
	{
		char name[0x8]{};

		static DirectoryName* ctor(DirectoryName* _thisptr)
		{
			if (!_thisptr)
				_thisptr = boss_new<DirectoryName>();
			memset(_thisptr->name, 0x00, 0x8);
			return _thisptr;
		}

		static const char* operator_const_char(DirectoryName* _thisptr)
		{
			return _thisptr->name;
		}
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

		static BossAccount* ctor(BossAccount* _this, uint32 accountId)
		{
			if (!_this)
				_this = boss_new<BossAccount>();
			_this->accountId = accountId;
			_this->vTablePtr = s_VTable;
			return _this;
		}

		static void dtor(BossAccount* _this, uint32 options)
		{
			if(_this && options & 1)
				boss_delete(_this);
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(BossAccount);
		}

	};
	static_assert(sizeof(BossAccount) == 8);

	struct TaskSetting
	{
		static const uint32 kBossCode = 0x7C0;
		static const uint32 kBossCodeLen = 0x20;

		static const uint32 kDirectorySizeLimit = 0x7F0;
		static const uint32 kDirectoryName = 0x7E0;
		static const uint32 kDirectoryNameLen = 0x8;

		//static const uint32 kFileName = 0x7F8;
		static const uint32 kNbdlFileName = 0x7F8;
		static const uint32 kFileNameLen = 0x20;

		static const uint32 kURL = 0x48;
		static const uint32 kURLLen = 0x100;

		static const uint32 kClientCert = 0x41;
		static const uint32 kCACert = 0x188;

		static const uint32 kServiceToken = 0x590;
		static const uint32 kServiceTokenLen = 0x200;

		uint8 settings[0x1000];
		MEMPTR<void> vTablePtr; // +0x1000

		struct VTableTaskSetting
		{
			VTableEntry rtti;
			VTableEntry dtor;
			VTableEntry RegisterPreprocess;
			VTableEntry unk1;
		};
		static inline SysAllocator<VTableTaskSetting> s_VTable;

		static TaskSetting* ctor(TaskSetting* _thisptr)
		{
			if(!_thisptr)
				_thisptr = boss_new<TaskSetting>();
			_thisptr->vTablePtr = s_VTable;
			InitializeSetting(_thisptr);
			return _thisptr;
		}

		static void dtor(TaskSetting* _this, uint32 options)
		{
			cemuLog_logDebug(LogType::Force, "nn::boss::TaskSetting::dtor(0x{:08x}, 0x{:08x})", MEMPTR(_this).GetMPTR(), options);
			if(options & 1)
				boss_delete(_this);
		}

		static bool IsPrivileged(TaskSetting* _thisptr)
		{
			const uint16 value = *(uint16be*)&_thisptr->settings[0x28];
			return value == 1 || value == 9 || value == 5;
		}

		static void InitializeSetting(TaskSetting* _thisptr)
		{
			memset(_thisptr, 0x00, sizeof(TaskSetting::settings));
			*(uint32*)&_thisptr->settings[0x0C] = 0;
			*(uint8*)&_thisptr->settings[0x2A] = 0x7D; // timeout?
			*(uint32*)&_thisptr->settings[0x30] = 0x7080;
			*(uint32*)&_thisptr->settings[0x8] = 0;
			*(uint32*)&_thisptr->settings[0x38] = 0;
			*(uint32*)&_thisptr->settings[0x3C] = 0x76A700;
			*(uint32*)&_thisptr->settings[0] = 0x76A700;
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(TaskSetting);
			s_VTable->RegisterPreprocess.ptr = nullptr; // todo
			s_VTable->unk1.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(TaskSetting) == 0x1004);
	static_assert(offsetof(TaskSetting, vTablePtr) == 0x1000);

	struct NetTaskSetting : TaskSetting
	{
		// 0x188 cert1 + 0x188 cert2 + 0x188 cert3
		// 0x190 AddCaCert (3times) char cert[0x80];
		// SetConnectionSetting
		// SetFirstLastModifiedTime

		struct VTableNetTaskSetting : public VTableTaskSetting
		{ };
		static inline SysAllocator<VTableNetTaskSetting> s_VTable;

		static Result AddCaCert(NetTaskSetting* _thisptr, const char* name)
		{
			if(name == nullptr || strnlen(name, 0x80) == 0x80)
			{
				cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddCaCert: name size is invalid");
				return 0xC0203780;
			}

			cemu_assert_unimplemented();

			return 0xA0220D00;
		}

		static NetTaskSetting* ctor(NetTaskSetting* _thisptr)
		{
			if (!_thisptr)
				_thisptr = boss_new<NetTaskSetting>();
			TaskSetting::ctor(_thisptr);
			*(uint32*)&_thisptr->settings[0x18C] = 0x78;
			_thisptr->vTablePtr = s_VTable;
			return _thisptr;
		}

		static Result SetServiceToken(NetTaskSetting* _thisptr, const uint8* serviceToken)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_SetServiceToken(0x{:x}, 0x{:x})", MEMPTR(_thisptr).GetMPTR(), MEMPTR(serviceToken).GetMPTR());
			cemuLog_logDebug(LogType::Force, "\t->{}", fmt::ptr(serviceToken));
			memcpy(&_thisptr->settings[TaskSetting::kServiceToken], serviceToken, TaskSetting::kServiceTokenLen);
			return 0x200080;
		}

		static Result AddInternalCaCert(NetTaskSetting* _thisptr, char certId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddInternalCaCert(0x{:x}, 0x{:x})", MEMPTR(_thisptr).GetMPTR(), (int)certId);

			uint32 location = TaskSetting::kCACert;
			for(int i = 0; i < 3; ++i)
			{
				if(_thisptr->settings[location] == 0)
				{
					_thisptr->settings[location] = (uint8)certId;
					return 0x200080;
				}

				location += TaskSetting::kCACert;
			}

			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddInternalCaCert: can't store certificate");
			return 0xA0220D00;
		}

		static void SetInternalClientCert(NetTaskSetting* _thisptr, char certId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_SetInternalClientCert(0x{:x}, 0x{:x})", MEMPTR(_thisptr).GetMPTR(), (int)certId);
			_thisptr->settings[TaskSetting::kClientCert] = (uint8)certId;
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(NetTaskSetting);
			s_VTable->RegisterPreprocess.ptr = nullptr; // todo
			s_VTable->unk1.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(NetTaskSetting) == 0x1004);

	struct NbdlTaskSetting : NetTaskSetting
	{
		struct VTableNbdlTaskSetting : public VTableNetTaskSetting
		{
			VTableEntry rttiNetTaskSetting; // unknown
		};
		static_assert(sizeof(VTableNbdlTaskSetting) == 8*5);
		static inline SysAllocator<VTableNbdlTaskSetting> s_VTable;

		static NbdlTaskSetting* ctor(NbdlTaskSetting* _thisptr)
		{
			if (!_thisptr)
				_thisptr = boss_new<NbdlTaskSetting>();
			NetTaskSetting::ctor(_thisptr);
			_thisptr->vTablePtr = s_VTable;
			return _thisptr;
		}

		static Result Initialize(NbdlTaskSetting* _thisptr, const char* bossCode, uint64 directorySizeLimit, const char* directoryName) // Initialize__Q3_2nn4boss15NbdlTaskSettingFPCcLT1
		{
			if(!bossCode || strnlen(bossCode, TaskSetting::kBossCodeLen) == TaskSetting::kBossCodeLen)
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780);

			if (directoryName && strnlen(directoryName, TaskSetting::kDirectoryNameLen) == TaskSetting::kDirectoryNameLen)
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780);

			strncpy((char*)&_thisptr->settings[TaskSetting::kBossCode], bossCode, TaskSetting::kBossCodeLen);

			*(uint64be*)&_thisptr->settings[TaskSetting::kDirectorySizeLimit] = directorySizeLimit; // uint64be
			if(directoryName)
				strncpy((char*)&_thisptr->settings[TaskSetting::kDirectoryName], directoryName, TaskSetting::kDirectoryNameLen);

			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
		}

		static Result SetFileName(NbdlTaskSetting* _thisptr, const char* fileName)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NbdlTaskSetting_t_SetFileName(0x{:08x}, {})", MEMPTR(_thisptr).GetMPTR(), fileName ? fileName : "\"\"");
			if (!fileName || strnlen(fileName, TaskSetting::kFileNameLen) == TaskSetting::kFileNameLen)
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780);

			strncpy((char*)&_thisptr->settings[TaskSetting::kNbdlFileName], fileName, TaskSetting::kFileNameLen);
			// also sets byte at +0x817 to zero?
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(NbdlTaskSetting);
			s_VTable->RegisterPreprocess.ptr = nullptr; // todo
			s_VTable->unk1.ptr = nullptr; // todo
			s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(NbdlTaskSetting) == 0x1004);

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

		static RawUlTaskSetting* ctor(RawUlTaskSetting* _thisptr)
		{
			if (!_thisptr)
				_thisptr = boss_new<RawUlTaskSetting>();
			NetTaskSetting::ctor(_thisptr);
			_thisptr->vTablePtr = s_VTable;
			_thisptr->ukRaw1 = 0;
			_thisptr->ukRaw2 = 0;
			_thisptr->ukRaw3 = 0;
			memset(_thisptr->rawSpace, 0x00, 0x200);
			return _thisptr;
		}

		static void dtor(RawUlTaskSetting* _this, uint32 options)
		{
			cemuLog_logDebug(LogType::Force, "nn::boss::RawUlTaskSetting::dtor() is todo");
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(RawUlTaskSetting);
			s_VTable->RegisterPreprocess.ptr = nullptr; // todo
			s_VTable->unk1.ptr = nullptr; // todo
			s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(RawUlTaskSetting) == 0x1210);

	struct RawDlTaskSetting : NetTaskSetting
	{
		struct VTableRawDlTaskSetting : public VTableNetTaskSetting
		{
			VTableEntry rttiNetTaskSetting; // unknown
		};
		static_assert(sizeof(VTableRawDlTaskSetting) == 8*5);
		static inline SysAllocator<VTableRawDlTaskSetting> s_VTable;

		static RawDlTaskSetting* ctor(RawDlTaskSetting* _thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_RawDlTaskSetting_ctor(0x{:x}) TODO", MEMPTR(_thisptr).GetMPTR());
			if (!_thisptr)
				_thisptr = boss_new<RawDlTaskSetting>();
			NetTaskSetting::ctor(_thisptr);
			_thisptr->vTablePtr = s_VTable;
			return _thisptr;
		}

		static Result Initialize(RawDlTaskSetting* _thisptr, const char* url, bool newArrival, bool led, const char* fileName, const char* directoryName)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_RawDlTaskSetting_Initialize(0x{:x}, 0x{:x}, {}, {}, 0x{:x}, 0x{:x})", MEMPTR(_thisptr).GetMPTR(), MEMPTR(url).GetMPTR(), newArrival, led, MEMPTR(fileName).GetMPTR(), MEMPTR(directoryName).GetMPTR());
			if (!url)
			{
				return 0xC0203780;
			}

			if (strnlen(url, TaskSetting::kURLLen) == TaskSetting::kURLLen)
			{
				return 0xC0203780;
			}

			cemuLog_logDebug(LogType::Force, "\t-> url: {}", url);

			if (fileName && strnlen(fileName, TaskSetting::kFileNameLen) == TaskSetting::kFileNameLen)
			{
				return 0xC0203780;
			}

			if (directoryName && strnlen(directoryName, TaskSetting::kDirectoryNameLen) == TaskSetting::kDirectoryNameLen)
			{
				return 0xC0203780;
			}

			strncpy((char*)_thisptr + TaskSetting::kURL, url, TaskSetting::kURLLen);
			_thisptr->settings[0x147] = '\0';

			if (fileName)
				strncpy((char*)_thisptr + 0x7D0, fileName, TaskSetting::kFileNameLen);
			else
				strncpy((char*)_thisptr + 0x7D0, "rawcontent.dat", TaskSetting::kFileNameLen);
			_thisptr->settings[0x7EF] = '\0';

			cemuLog_logDebug(LogType::Force, "\t-> filename: {}", (char*)_thisptr + 0x7D0);

			if (directoryName)
			{
				strncpy((char*)_thisptr + 0x7C8, directoryName, TaskSetting::kDirectoryNameLen);
				_thisptr->settings[0x7CF] = '\0';
				cemuLog_logDebug(LogType::Force, "\t-> directoryName: {}", (char*)_thisptr + 0x7C8);
			}

			_thisptr->settings[0x7C0] = newArrival;
			_thisptr->settings[0x7C1] = led;
			*(uint16be*)&_thisptr->settings[0x28] = 0x3;
			return 0x200080;
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(RawDlTaskSetting);
			s_VTable->RegisterPreprocess.ptr = nullptr; // todo
			s_VTable->unk1.ptr = nullptr; // todo
			s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(RawDlTaskSetting) == 0x1004);

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

		static PlayReportSetting* ctor(PlayReportSetting* _this)
		{
			if(!_this)
				_this = boss_new<PlayReportSetting>();
			RawUlTaskSetting::ctor(_this);
			_this->vTablePtr = s_VTable;
			_this->ukn1210_ptr = nullptr;
			_this->ukn1214_size = 0;
			_this->ukPlay3 = 0;
			_this->ukPlay4 = 0;
			return _this;
		}

		static void dtor(PlayReportSetting* _this, uint32 options)
		{
			RawUlTaskSetting::dtor(_this, 0);
			if(options&1)
				boss_delete(_this->ukn1210_ptr.GetPtr());
		}

		static void Initialize(PlayReportSetting* _this, uint8* ptr, uint32 size)
		{
			if(!ptr || size == 0 || size > 0x19000)
			{
				cemuLog_logDebug(LogType::Force, "nn::boss::PlayReportSetting::Initialize: invalid parameter");
				return;
			}

			*ptr = 0;

			*(uint16be*)&_this->settings[0x28] = 6;
			*(uint16be*)&_this->settings[0x2B] |= 0x3;
			*(uint16be*)&_this->settings[0x2C] |= 0xA;
			*(uint32be*)&_this->settings[0x7C0] |= 2;

			_this->ukn1210_ptr = ptr;
			_this->ukn1214_size = size;
			_this->ukPlay3 = 0;
			_this->ukPlay4 = 0;

			// TODO
		}

		static bool Set(PlayReportSetting* _this, const char* keyname, uint32 value)
		{
			// TODO
			return true;
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(PlayReportSetting);
			s_VTable->RegisterPreprocess.ptr = nullptr; // todo
			s_VTable->unk1.ptr = nullptr; // todo
			s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(PlayReportSetting) == 0x1220);

	struct Task
	{
		struct VTableTask
		{
			VTableEntry rtti;
			VTableEntry dtor;
		};
		static inline SysAllocator<VTableTask> s_vTable;

		uint32be accountId; // 0x00
		uint32be uk2; // 0x04
		TaskId taskId; // 0x08
		TitleId titleId; // 0x10
		MEMPTR<VTableTask> vTablePtr; // 0x18
		uint32be padding; // 0x1C

		static Result Initialize1(Task* _thisptr, const char* taskId, uint32 accountId) // Initialize__Q3_2nn4boss4TaskFPCcUi
		{
			if(!taskId || strnlen(taskId, 0x8) == 8)
			{
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780);
			}
			_thisptr->accountId = accountId;
			strncpy(_thisptr->taskId.id, taskId, 0x08);
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
		}

		static Result Initialize2(Task* _thisptr, uint8 slot, const char* taskId) // Initialize__Q3_2nn4boss4TaskFUcPCc
		{
			const uint32 accountId = slot == 0 ? 0 : act::GetPersistentIdEx(slot);
			return Initialize1(_thisptr, taskId, accountId);
		}

		static Result Initialize3(Task* _thisptr, const char* taskId) // Initialize__Q3_2nn4boss4TaskFPCc
		{
			return Initialize1(_thisptr, taskId, 0);
		}

		static Task* ctor2(Task* _thisptr, const char* taskId, uint32 accountId) // __ct__Q3_2nn4boss4TaskFPCcUi
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->accountId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctor(&_thisptr->taskId);
			TitleId::ctor(&_thisptr->titleId, 0);
			auto r = Initialize1(_thisptr, taskId, accountId);
			cemu_assert_debug(NN_RESULT_IS_SUCCESS(r));
			return _thisptr;
		}

		static Task* ctor1(Task* _thisptr, uint8 slot, const char* taskId) // __ct__Q3_2nn4boss4TaskFUcPCc
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->accountId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctor(&_thisptr->taskId);
			TitleId::ctor(&_thisptr->titleId, 0);
			auto r = Initialize2(_thisptr, slot, taskId);
			cemu_assert_debug(NN_RESULT_IS_SUCCESS(r));
			return _thisptr;
		}

		static Task* ctor3(Task* _thisptr, const char* taskId) // __ct__Q3_2nn4boss4TaskFPCc
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->accountId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctor(&_thisptr->taskId);
			TitleId::ctor(&_thisptr->titleId, 0);
			auto r = Initialize3(_thisptr, taskId);
			cemu_assert_debug(NN_RESULT_IS_SUCCESS(r));
			return _thisptr;
		}

		static Task* ctor4(Task* _thisptr) // __ct__Q3_2nn4boss4TaskFv
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->accountId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctor(&_thisptr->taskId);
			TitleId::ctor(&_thisptr->titleId, 0);
			memset(&_thisptr->taskId, 0x00, sizeof(TaskId));
			return _thisptr;
		}

		static void dtor(Task* _this, uint32 options) // __dt__Q3_2nn4boss4TaskFv
		{
			cemuLog_logDebug(LogType::Force, "nn::boss::Task::dtor(0x{:08x}, 0x{:08x})", MEMPTR(_this).GetMPTR(), options);
			// todo - Task::Finalize
			if(options & 1)
				boss_delete(_this);
		}

		static Result Run(Task* _thisptr, bool isForegroundRun)
		{
			if (isForegroundRun != 0)
			{
				cemuLog_logDebug(LogType::Force, "export_Run foreground run");
			}

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_RUN;
			bossRequest->accountId = _thisptr->accountId;
			bossRequest->taskId = _thisptr->taskId.id;
			bossRequest->titleId = _thisptr->titleId.u64;
			bossRequest->bool_parameter = isForegroundRun != 0;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			return 0;
		}

		static Result StartScheduling(Task* _thisptr, uint8 executeImmediately)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_START_SCHEDULING;
			bossRequest->accountId = _thisptr->accountId;
			bossRequest->taskId = _thisptr->taskId.id;
			bossRequest->titleId = _thisptr->titleId.u64;
			bossRequest->bool_parameter = executeImmediately != 0;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			return 0;
		}

		static Result StopScheduling(Task* _thisptr)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_STOP_SCHEDULING;
			bossRequest->accountId = _thisptr->accountId;
			bossRequest->taskId = _thisptr->taskId.id;
			bossRequest->titleId = _thisptr->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			return 0;
		}

		static Result IsRegistered(Task* _thisptr)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_IS_REGISTERED;
			bossRequest->accountId = _thisptr->accountId;
			bossRequest->titleId = _thisptr->titleId.u64;
			bossRequest->taskId = _thisptr->taskId.id;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			return bossRequest->returnCode;
		}

		static Result Wait(Task* _thisptr, uint32 timeout, uint32 waitState) // Wait__Q3_2nn4boss4TaskFUiQ3_2nn4boss13TaskWaitState
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_WAIT;
			bossRequest->titleId = _thisptr->titleId.u64;
			bossRequest->taskId = _thisptr->taskId.id;
			bossRequest->timeout = timeout;
			bossRequest->waitState = waitState;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			return bossRequest->returnCode;
		}

		static Result RegisterForImmediateRun(Task* _thisptr, TaskSetting* settings) // RegisterForImmediateRun__Q3_2nn4boss4TaskFRCQ3_2nn4boss11TaskSetting
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_REGISTER;
			bossRequest->accountId = _thisptr->accountId;
			bossRequest->taskId = _thisptr->taskId.id;
			bossRequest->settings = settings;
			bossRequest->uk1 = 0xC00;

			if (TaskSetting::IsPrivileged(settings))
				bossRequest->titleId = _thisptr->titleId.u64;

			Result result = __depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);
			return result;
		}

		static Result Unregister(Task* _thisptr)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_UNREGISTER;
			bossRequest->accountId = _thisptr->accountId;
			bossRequest->taskId = _thisptr->taskId.id;
			bossRequest->titleId = _thisptr->titleId.u64;

			const sint32 result = __depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);
			return result;
		}

		static Result Register(Task* _thisptr, TaskSetting* settings)
		{
			if (!settings)
			{
				cemuLog_logDebug(LogType::Force, "nn_boss_Task_Register - crash workaround (fix me)"); // settings should never be zero
				return 0;
			}

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_REGISTER_FOR_IMMEDIATE_RUN;
			bossRequest->accountId = _thisptr->accountId;
			bossRequest->taskId = _thisptr->taskId.id;
			bossRequest->settings = settings;
			bossRequest->uk1 = 0xC00;

			if(TaskSetting::IsPrivileged(settings))
				bossRequest->titleId = _thisptr->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			return bossRequest->returnCode;
		}

		static uint32 GetTurnState(Task* _this, uint32be* executionCountOut)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_TURN_STATE;
			bossRequest->accountId = _this->accountId;
			bossRequest->taskId = _this->taskId.id;
			bossRequest->titleId = _this->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (executionCountOut)
				*executionCountOut = bossRequest->u32.exec_count;

			return bossRequest->u32.result;
			// 7 -> finished? 0x11 -> Error (Splatoon doesn't like it when we return 0x11 for Nbdl tasks)
		}

		static uint64 GetContentLength(Task* _this, uint32be* executionCountOut)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_CONTENT_LENGTH;
			bossRequest->accountId = _this->accountId;
			bossRequest->taskId = _this->taskId.id;
			bossRequest->titleId = _this->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (executionCountOut)
				*executionCountOut = bossRequest->u64.exec_count;

			return bossRequest->u64.result;
		}

		static uint64 GetProcessedLength(Task* _this, uint32be* executionCountOut)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_PROCESSED_LENGTH;
			bossRequest->accountId = _this->accountId;
			bossRequest->taskId = _this->taskId.id;
			bossRequest->titleId = _this->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (executionCountOut)
				*executionCountOut = bossRequest->u64.exec_count;
			return bossRequest->u64.result;
		}

		static uint32 GetHttpStatusCode(Task* _this, uint32be* executionCountOut)
		{
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_HTTP_STATUS_CODE;
			bossRequest->accountId = _this->accountId;
			bossRequest->taskId = _this->taskId.id;
			bossRequest->titleId = _this->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (executionCountOut)
				*executionCountOut = bossRequest->u32.exec_count;

			return bossRequest->u32.result;
		}

		static void InitVTable()
		{
			s_vTable->rtti.ptr = nullptr; // todo
			s_vTable->dtor.ptr = RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) { Task::dtor(MEMPTR<Task>(hCPU->gpr[3]), hCPU->gpr[4]); osLib_returnFromFunction(hCPU, 0); });
		}
	};

	static_assert(sizeof(Task) == 0x20);

	struct PrivilegedTask : Task
	{
		struct VTablePrivilegedTask : public VTableTask
		{
			VTableEntry rttiTask;
		};
		static_assert(sizeof(VTablePrivilegedTask) == 8*3);
		static inline SysAllocator<VTablePrivilegedTask> s_VTable;

		static PrivilegedTask* ctor(PrivilegedTask* _thisptr)
		{
			if (!_thisptr)
				_thisptr = boss_new<PrivilegedTask>();
			Task::ctor4(_thisptr);
			_thisptr->vTablePtr = s_VTable;
			return _thisptr;
		}

		static void dtor(PrivilegedTask* _this, uint32 options)
		{
			if(!_this)
				return;
			Task::dtor(_this, 0);
			if(options & 1)
				boss_delete(_this);
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(PrivilegedTask);
			s_VTable->rttiTask.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(PrivilegedTask) == 0x20);

	struct AlmightyTask : PrivilegedTask
	{
		struct VTableAlmightyTask : public VTablePrivilegedTask
		{};
		static_assert(sizeof(VTableAlmightyTask) == 8*3);
		static inline SysAllocator<VTableAlmightyTask> s_VTable;

		static AlmightyTask* ctor(AlmightyTask* _thisptr)
		{
			if (!_thisptr)
				_thisptr = boss_new<AlmightyTask>();
			PrivilegedTask::ctor(_thisptr);
			_thisptr->vTablePtr = s_VTable;
			return _thisptr;
		}

		static void dtor(AlmightyTask* _thisptr, uint32 options)
		{
			if (!_thisptr)
				return;
			PrivilegedTask::dtor(_thisptr, 0);
			if(options&1)
				boss_delete(_thisptr);
		}

		static uint32 Initialize(AlmightyTask* _thisptr, TitleId* titleId, const char* taskId, uint32 accountId)
		{
			if (!_thisptr)
				return 0xc0203780;

			_thisptr->accountId = accountId;
			_thisptr->titleId.u64 = titleId->u64;
			strncpy(_thisptr->taskId.id, taskId, 8);
			_thisptr->taskId.id[7] = 0x00;

			return 0x200080;
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(AlmightyTask);
			s_VTable->rttiTask.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(AlmightyTask) == 0x20);

	struct DataName
	{
		char name[32];

		static DataName* ctor(DataName* _this) // __ct__Q3_2nn4boss8DataNameFv
		{
			if(!_this)
				_this = boss_new<DataName>();
			memset(_this->name, 0, sizeof(name));
			return _this;
		}

		static const char* operator_const_char(DataName* _this) // __opPCc__Q3_2nn4boss8DataNameCFv
		{
			return _this->name;
		}
	};
	static_assert(sizeof(DataName) == 0x20);

	struct BossStorageFadEntry
	{
		char name[32];
		uint32be fileNameId;
		uint32 ukn24;
		uint32 ukn28;
		uint32 ukn2C;
		uint32 ukn30;
		uint32be timestampRelated; // guessed
	};

#define FAD_ENTRY_MAX_COUNT		512

	struct Storage
	{
		struct VTableStorage
		{
			VTableEntry rtti;
			VTableEntry dtor;
		};
		static inline SysAllocator<VTableStorage> s_vTable;

		enum StorageKind
		{
			kStorageKind_NBDL,
			kStorageKind_RawDl,
		};

		/* +0x00 */ uint32be accountId;
		/* +0x04 */ uint32be storageKind;
		/* +0x08 */ uint8 ukn08Array[3];
		/* +0x0B */ char storageName[8];
		uint8 ukn13;
		uint8 ukn14;
		uint8 ukn15;
		uint8 ukn16;
		uint8 ukn17;
		/* +0x18 */ nn::boss::TitleId titleId;
		/* +0x20 */ MEMPTR<VTableStorage> vTablePtr;
		/* +0x24 */ uint32be ukn24;

		static nn::boss::Storage* ctor1(nn::boss::Storage* _this) // __ct__Q3_2nn4boss7StorageFv
		{
			if(!_this)
				_this = boss_new<nn::boss::Storage>();
			_this->vTablePtr = s_vTable;
			_this->titleId.u64 = 0;
			return _this;
		}

		static void dtor(nn::boss::Storage* _this, uint32 options) // __dt__Q3_2nn4boss7StorageFv
		{
			cemuLog_logDebug(LogType::Force, "nn::boss::Storage::dtor(0x{:08x}, 0x{:08x})", MEMPTR(_this).GetMPTR(), options);
			Finalize(_this);
			if(options & 1)
				boss_delete(_this);
		}

		static void nnBossStorage_prepareTitleId(Storage* storage)
		{
			if (storage->titleId.u64 != 0)
				return;
			storage->titleId.u64 = CafeSystem::GetForegroundTitleId();
		}

		static Result Initialize(Storage* _thisptr, const char* dirName, uint32 accountId, StorageKind type)
		{
			if (!dirName)
				return 0xC0203780;

			cemuLog_logDebug(LogType::Force, "boss::Storage::Initialize({}, 0x{:08x}, {})", dirName, accountId, type);

			_thisptr->storageKind = type;
			_thisptr->titleId.u64 = 0;

			memset(_thisptr->storageName, 0, 0x8);
			strncpy(_thisptr->storageName, dirName, 0x8);
			_thisptr->storageName[7] = '\0';

			_thisptr->accountId = accountId;

			nnBossStorage_prepareTitleId(_thisptr); // usually not done like this

			return 0x200080;
		}

		static Result Initialize2(Storage* _thisptr, const char* dirName, StorageKind type)
		{
			return Initialize(_thisptr, dirName, 0, type);
		}

		static void Finalize(Storage* _this)
		{
			memset(_this, 0, sizeof(Storage)); // todo - not all fields might be cleared
		}

		static Result GetDataList(nn::boss::Storage* storage, DataName* dataList, sint32 maxEntries, uint32be* outputEntryCount, uint32 startIndex) // GetDataList__Q3_2nn4boss7StorageCFPQ3_2nn4boss8DataNameUiPUiT2
		{
			// initialize titleId of storage if not already done
			nnBossStorage_prepareTitleId(storage);

			if(startIndex >= FAD_ENTRY_MAX_COUNT) {
				*outputEntryCount = 0;
				return 0;
			}

			// load fad.db
			BossStorageFadEntry* fadTable = nnBossStorageFad_getTable(storage);
			if (fadTable)
			{
				sint32 validEntryCount = 0;
				for (sint32 i = startIndex; i < FAD_ENTRY_MAX_COUNT; i++)
				{
					if( fadTable[i].name[0] == '\0' )
						continue;
					memcpy(dataList[validEntryCount].name, fadTable[i].name, 0x20);
					validEntryCount++;
					if (validEntryCount >= maxEntries)
						break;
				}
				*outputEntryCount = validEntryCount;
				free(fadTable);
			}
			else
			{
				// could not load fad table
				*outputEntryCount = 0;
			}
			return 0; // todo
		}

		static bool Exist(nn::boss::Storage* storage)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss::Storage::Exist() TODO");
			return true;
		}

		/* FAD access */

		static FSCVirtualFile* nnBossStorageFile_open(nn::boss::Storage* storage, uint32 fileNameId)
		{
			char storageFilePath[1024];
			sprintf(storageFilePath, "/cemuBossStorage/%08x/%08x/user/common/data/%s/%08x", (uint32)(storage->titleId.u64 >> 32), (uint32)(storage->titleId.u64), storage->storageName, fileNameId);
			sint32 fscStatus;
			FSCVirtualFile* fscStorageFile = fsc_open(storageFilePath, FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION | FSC_ACCESS_FLAG::WRITE_PERMISSION, &fscStatus);
			return fscStorageFile;
		}

		static BossStorageFadEntry* nnBossStorageFad_getTable(nn::boss::Storage* storage)
		{
			const auto accountId = ActiveSettings::GetPersistentId();
			char fadPath[1024];
			sprintf(fadPath, "/cemuBossStorage/%08x/%08x/user/common/%08x/%s/fad.db", (uint32)(storage->titleId.u64 >> 32), (uint32)(storage->titleId.u64), accountId, storage->storageName);

			sint32 fscStatus;
			FSCVirtualFile* fscFadFile = fsc_open(fadPath, FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
			if (!fscFadFile)
			{
				return nullptr;
			}
			// skip first 8 bytes
			fsc_setFileSeek(fscFadFile, 8);
			// read entries
			BossStorageFadEntry* fadTable = (BossStorageFadEntry*)malloc(sizeof(BossStorageFadEntry)*FAD_ENTRY_MAX_COUNT);
			memset(fadTable, 0, sizeof(BossStorageFadEntry)*FAD_ENTRY_MAX_COUNT);
			fsc_readFile(fscFadFile, fadTable, sizeof(BossStorageFadEntry)*FAD_ENTRY_MAX_COUNT);
			fsc_close(fscFadFile);
			return fadTable;
		}

		// Find index of entry by name. Returns -1 if not found
		static sint32 nnBossStorageFad_getIndexByName(BossStorageFadEntry* fadTable, char* name)
		{
			for (sint32 i = 0; i < FAD_ENTRY_MAX_COUNT; i++)
			{
				if (fadTable[i].name[0] == '\0')
					continue;
				if (strncmp(name, fadTable[i].name, 0x20) == 0)
				{
					return i;
				}
			}
			return -1;
		}

		static bool nnBossStorageFad_getEntryByName(nn::boss::Storage* storage, char* name, BossStorageFadEntry* fadEntry)
		{
			BossStorageFadEntry* fadTable = nnBossStorageFad_getTable(storage);
			if (fadTable)
			{
				sint32 entryIndex = nnBossStorageFad_getIndexByName(fadTable, name);
				if (entryIndex >= 0)
				{
					memcpy(fadEntry, fadTable + entryIndex, sizeof(BossStorageFadEntry));
					free(fadTable);
					return true;
				}
				free(fadTable);
			}
			return false;
		}

		static void InitVTable()
		{
			s_vTable->rtti.ptr = nullptr; // todo
			s_vTable->dtor.ptr = DTOR_WRAPPER(Storage);
		}
	};

	static_assert(sizeof(Storage) == 0x28);
	static_assert(offsetof(Storage, storageKind) == 0x04);
	static_assert(offsetof(Storage, ukn08Array) == 0x08);
	static_assert(offsetof(Storage, storageName) == 0x0B);
	static_assert(offsetof(Storage, titleId) == 0x18);

	struct AlmightyStorage : Storage
	{
		struct VTableAlmightyStorage : public VTableStorage
		{
			VTableEntry rttiStorage;
		};
		static_assert(sizeof(VTableAlmightyStorage) == 8*3);
		static inline SysAllocator<VTableAlmightyStorage> s_VTable;

		static AlmightyStorage* ctor(AlmightyStorage* _thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_AlmightyStorage_ctor(0x{:x})", MEMPTR(_thisptr).GetMPTR());
			if (!_thisptr)
				_thisptr = boss_new<AlmightyStorage>();
			Storage::ctor1(_thisptr);
			_thisptr->vTablePtr = s_VTable;
			return _thisptr;
		}

		static uint32 Initialize(AlmightyStorage* _thisptr, TitleId* titleId, const char* storageName, uint32 accountId, StorageKind storageKind)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_AlmightyStorage_Initialize(0x{:x})", MEMPTR(_thisptr).GetMPTR());
			if (!_thisptr)
				return 0xc0203780;

			_thisptr->accountId = accountId;
			_thisptr->storageKind = storageKind;
			_thisptr->titleId.u64 = titleId->u64;

			strncpy(_thisptr->storageName, storageName, 8);
			_thisptr->storageName[0x7] = 0x00;

			return 0x200080;
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(AlmightyStorage);
			s_VTable->rttiStorage.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(AlmightyStorage) == 0x28);

	// NsData

	struct NsData
	{
		struct VTableNsData
		{
			VTableEntry rtti;
			VTableEntry dtor;
		};
		static inline SysAllocator<VTableNsData> s_vTable;

		/* +0x00 */ char name[0x20]; // DataName ?
		/* +0x20 */ nn::boss::Storage storage;
		/* +0x48 */ uint64 readIndex;
		/* +0x50 */ MEMPTR<void> vTablePtr;
		/* +0x54 */ uint32 ukn54;

		static NsData* ctor(NsData* _this)
		{
			if (!_this)
				_this = boss_new<NsData>();
			_this->vTablePtr = s_vTable;
			memset(_this->name, 0, sizeof(_this->name));
			_this->storage.ctor1(&_this->storage);
			_this->readIndex = 0;
			return _this;
		}

		static void dtor(NsData* _this, uint32 options) // __dt__Q3_2nn4boss6NsDataFv
		{
			_this->storage.dtor(&_this->storage, 0);
			// todo
			if(options & 1)
				boss_delete(_this);
		}

		static Result Initialize(NsData* _this, nn::boss::Storage* storage, const char* dataName)
		{
			if(dataName == nullptr)
			{
				if (storage->storageKind != 1)
				{
					return 0xC0203780;
				}
			}

			_this->storage.accountId = storage->accountId;
			_this->storage.storageKind = storage->storageKind;

			memcpy(_this->storage.ukn08Array, storage->ukn08Array, 3);
			memcpy(_this->storage.storageName, storage->storageName, 8);

			_this->storage.titleId.u64 = storage->titleId.u64;

			_this->storage = *storage;

			if (dataName != nullptr || storage->storageKind != 1)
				strncpy(_this->name, dataName, 0x20);
			else
				strncpy(_this->name, "rawcontent.dat", 0x20);
			_this->name[0x1F] = '\0';

			_this->readIndex = 0;

			cemuLog_logDebug(LogType::Force, "initialize: {}", _this->name);

			return 0x200080;
		}

		static std::string _GetPath(NsData* nsData)
		{
			uint32 accountId = nsData->storage.accountId;
			if (accountId == 0)
				accountId = iosuAct_getAccountIdOfCurrentAccount();

			uint64 title_id = nsData->storage.titleId.u64;
			if (title_id == 0)
				title_id = CafeSystem::GetForegroundTitleId();

			fs::path path = fmt::format("cemuBossStorage/{:08x}/{:08x}/user/{:08x}", (uint32)(title_id >> 32), (uint32)(title_id & 0xFFFFFFFF), accountId);
			path /= nsData->storage.storageName;
			path /= nsData->name;
			return path.string();
		}

		static Result DeleteRealFileWithHistory(NsData* nsData)
		{
			if (nsData->storage.storageKind == nn::boss::Storage::kStorageKind_NBDL)
			{
				// todo
				cemuLog_log(LogType::Force, "BOSS NBDL: Unsupported delete");
			}
			else
			{
				sint32 fscStatus = FSC_STATUS_OK;
				std::string filePath = _GetPath(nsData).c_str();
				fsc_remove((char*)filePath.c_str(), &fscStatus);
				if (fscStatus != 0)
					cemuLog_log(LogType::Force, "Unhandeled FSC status in BOSS DeleteRealFileWithHistory()");
			}
			return 0;
		}

		static uint32 Exist(NsData* nsData)
		{
			bool fileExists = false;
			if(nsData->storage.storageKind == nn::boss::Storage::kStorageKind_NBDL)
			{
				// check if name is present in fad table
				BossStorageFadEntry* fadTable = nn::boss::Storage::nnBossStorageFad_getTable(&nsData->storage);
				if (fadTable)
				{
					fileExists = nn::boss::Storage::nnBossStorageFad_getIndexByName(fadTable, nsData->name) >= 0;
					cemuLog_logDebug(LogType::Force, "\t({}) -> {}", nsData->name, fileExists);
					free(fadTable);
				}
			}
			else
			{
				sint32 fscStatus;
				auto fscStorageFile = fsc_open(_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE, &fscStatus);
				if (fscStorageFile != nullptr)
				{
					fileExists = true;
					fsc_close(fscStorageFile);
				}
			}
			return fileExists?1:0;
		}

		static uint64 GetSize(NsData* nsData)
		{
			FSCVirtualFile* fscStorageFile = nullptr;
			if (nsData->storage.storageKind == nn::boss::Storage::kStorageKind_NBDL)
			{
				BossStorageFadEntry fadEntry;
				if (nn::boss::Storage::nnBossStorageFad_getEntryByName(&nsData->storage, nsData->name, &fadEntry) == false)
				{
					cemuLog_log(LogType::Force, "BOSS storage cant find file {}", nsData->name);
					return 0;
				}
				// open file
				fscStorageFile = nn::boss::Storage::nnBossStorageFile_open(&nsData->storage, fadEntry.fileNameId);
			}
			else
			{
				sint32 fscStatus;
				fscStorageFile = fsc_open(_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
			}

			if (fscStorageFile == nullptr)
			{
				cemuLog_log(LogType::Force, "BOSS storage cant open file alias {}", nsData->name);
				return 0;
			}

			// get size
			const sint32 fileSize = fsc_getFileSize(fscStorageFile);
			// close file
			fsc_close(fscStorageFile);
			return fileSize;
		}

		static uint64 GetCreatedTime(NsData* nsData)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss.NsData_GetCreatedTime() not implemented. Returning 0");
			uint64 createdTime = 0;
			return createdTime;
		}

		static uint32 nnBossNsData_read(NsData* nsData, uint64be* sizeOutBE, void* buffer, sint32 length)
		{
			FSCVirtualFile* fscStorageFile = nullptr;
			if (nsData->storage.storageKind == nn::boss::Storage::kStorageKind_NBDL)
			{
				BossStorageFadEntry fadEntry;
				if (nn::boss::Storage::nnBossStorageFad_getEntryByName(&nsData->storage, nsData->name, &fadEntry) == false)
				{
					cemuLog_log(LogType::Force, "BOSS storage cant find file {} for reading", nsData->name);
					return 0x80000000; // todo - proper error code
				}
				// open file
				fscStorageFile = nn::boss::Storage::nnBossStorageFile_open(&nsData->storage, fadEntry.fileNameId);
			}
			else
			{
				sint32 fscStatus;
				fscStorageFile = fsc_open(_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
			}

			if (!fscStorageFile)
			{
				cemuLog_log(LogType::Force, "BOSS storage cant open file alias {} for reading", nsData->name);
				return 0x80000000; // todo - proper error code
			}
			// get size
			sint32 fileSize = fsc_getFileSize(fscStorageFile);
			// verify read is within bounds
			sint32 readEndOffset = (sint32)_swapEndianU64(nsData->readIndex) + length;
			sint32 readBytes = length;
			if (readEndOffset > fileSize)
			{
				readBytes = fileSize - (sint32)_swapEndianU64(nsData->readIndex);
				cemu_assert_debug(readBytes != 0);
			}
			// read
			fsc_setFileSeek(fscStorageFile, (uint32)_swapEndianU64(nsData->readIndex));
			fsc_readFile(fscStorageFile, buffer, readBytes);
			nsData->readIndex = _swapEndianU64((sint32)_swapEndianU64(nsData->readIndex) + readBytes);

			// close file
			fsc_close(fscStorageFile);
			if (sizeOutBE)
				*sizeOutBE = readBytes;
			return 0;
		}

#define NSDATA_SEEK_MODE_BEGINNING	(0)

		static uint32 nnBossNsData_seek(NsData* nsData, uint64 seek, uint32 mode)
		{
			FSCVirtualFile* fscStorageFile = nullptr;
			if (nsData->storage.storageKind == nn::boss::Storage::kStorageKind_NBDL)
			{
				BossStorageFadEntry fadEntry;
				if (nn::boss::Storage::nnBossStorageFad_getEntryByName(&nsData->storage, nsData->name, &fadEntry) == false)
				{
					cemuLog_log(LogType::Force, "BOSS storage cant find file {} for reading", nsData->name);
					return 0x80000000; // todo - proper error code
				}
				// open file
				fscStorageFile = nn::boss::Storage::nnBossStorageFile_open(&nsData->storage, fadEntry.fileNameId);
			}
			else
			{
				sint32 fscStatus;
				fscStorageFile = fsc_open(_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
			}

			if (fscStorageFile == nullptr)
			{
				cemuLog_log(LogType::Force, "BOSS storage cant open file alias {} for reading",  nsData->name);
				return 0x80000000; // todo - proper error code
			}
			// get size
			sint32 fileSize = fsc_getFileSize(fscStorageFile);
			// handle seek
			if (mode == NSDATA_SEEK_MODE_BEGINNING)
			{
				seek = std::min(seek, (uint64)fileSize);
				nsData->readIndex = _swapEndianU64((uint64)seek);
			}
			else
			{
				cemu_assert_unimplemented();
			}
			fsc_close(fscStorageFile);
			return 0;
		}

		static sint32 Read(NsData* nsData, uint8* buffer, sint32 length)
		{
			cemuLog_logDebug(LogType::Force, "nsData read (filename {})", nsData->name);
			return nnBossNsData_read(nsData, nullptr, buffer, length);
		}

		static sint32 ReadWithSizeOut(NsData* nsData, uint64be* sizeOut, uint8* buffer, sint32 length)
		{
			uint32 r = nnBossNsData_read(nsData, sizeOut, buffer, length);
			cemuLog_logDebug(LogType::Force, "nsData readWithSizeOut (filename {} length 0x{:x}) Result: {} Sizeout: {:x}", nsData->name, length, r, _swapEndianU64(*sizeOut));
			return r;
		}

		static Result Seek(NsData* nsData, uint64 seekPos, uint32 mode)
		{
			uint32 r = nnBossNsData_seek(nsData, seekPos, mode);
			cemuLog_logDebug(LogType::Force, "nsData seek (filename {} seek 0x{:x}) Result: {}", nsData->name, (uint32)seekPos, r);
			return r;
		}

		static void InitVTable()
		{
			s_vTable->rtti.ptr = nullptr; // todo
			s_vTable->dtor.ptr = DTOR_WRAPPER(NsData);
		}
	};
	static_assert(sizeof(NsData) == 0x58);

}
}
void nnBoss_load()
{
	OSInitMutexEx(&nn::boss::g_mutex, nullptr);

	nn::boss::g_initCounter = 0;
	nn::boss::g_isInitialized = false;

	cafeExportRegisterFunc(nn::boss::GetBossState, "nn_boss", "GetBossState__Q2_2nn4bossFv", LogType::NN_BOSS);

	// boss lib
	cafeExportRegisterFunc(nn::boss::Initialize, "nn_boss", "Initialize__Q2_2nn4bossFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::IsInitialized, "nn_boss", "IsInitialized__Q2_2nn4bossFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Finalize, "nn_boss", "Finalize__Q2_2nn4bossFv", LogType::NN_BOSS);

	// task
	nn::boss::Task::InitVTable();
	cafeExportRegisterFunc(nn::boss::Task::ctor1, "nn_boss", "__ct__Q3_2nn4boss4TaskFUcPCc", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::ctor2, "nn_boss", "__ct__Q3_2nn4boss4TaskFPCcUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::ctor3, "nn_boss", "__ct__Q3_2nn4boss4TaskFPCc", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::ctor4, "nn_boss", "__ct__Q3_2nn4boss4TaskFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::dtor, "nn_boss", "__dt__Q3_2nn4boss4TaskFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::Initialize1, "nn_boss", "Initialize__Q3_2nn4boss4TaskFPCcUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::Initialize2, "nn_boss", "Initialize__Q3_2nn4boss4TaskFUcPCc", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::Initialize3, "nn_boss", "Initialize__Q3_2nn4boss4TaskFPCc", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::Run, "nn_boss", "Run__Q3_2nn4boss4TaskFb", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::Wait, "nn_boss", "Wait__Q3_2nn4boss4TaskFUiQ3_2nn4boss13TaskWaitState", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::GetTurnState, "nn_boss", "GetTurnState__Q3_2nn4boss4TaskCFPUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::GetHttpStatusCode, "nn_boss", "GetHttpStatusCode__Q3_2nn4boss4TaskCFPUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::GetContentLength, "nn_boss", "GetContentLength__Q3_2nn4boss4TaskCFPUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::GetProcessedLength, "nn_boss", "GetProcessedLength__Q3_2nn4boss4TaskCFPUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::Register, "nn_boss", "Register__Q3_2nn4boss4TaskFRQ3_2nn4boss11TaskSetting", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::Unregister, "nn_boss", "Unregister__Q3_2nn4boss4TaskFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::IsRegistered, "nn_boss", "IsRegistered__Q3_2nn4boss4TaskCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::RegisterForImmediateRun, "nn_boss", "RegisterForImmediateRun__Q3_2nn4boss4TaskFRCQ3_2nn4boss11TaskSetting", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::StartScheduling, "nn_boss", "StartScheduling__Q3_2nn4boss4TaskFb", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Task::StopScheduling, "nn_boss", "StopScheduling__Q3_2nn4boss4TaskFv", LogType::NN_BOSS);

	// TaskSetting
	nn::boss::TaskSetting::InitVTable();
	cafeExportRegisterFunc(nn::boss::TaskSetting::ctor, "nn_boss", "__ct__Q3_2nn4boss11TaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TaskSetting::dtor, "nn_boss", "__dt__Q3_2nn4boss11TaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TaskSetting::IsPrivileged, "nn_boss", "Initialize__Q3_2nn4boss11TaskSettingFPCcUi", LogType::NN_BOSS);

	// NbdlTaskSetting
	nn::boss::NbdlTaskSetting::InitVTable();
	cafeExportRegisterFunc(nn::boss::NbdlTaskSetting::ctor, "nn_boss", "__ct__Q3_2nn4boss15NbdlTaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NbdlTaskSetting::dtor, "nn_boss", "__dt__Q3_2nn4boss15NbdlTaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NbdlTaskSetting::Initialize, "nn_boss", "Initialize__Q3_2nn4boss15NbdlTaskSettingFPCcLT1", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NbdlTaskSetting::SetFileName, "nn_boss", "SetFileName__Q3_2nn4boss15NbdlTaskSettingFPCc", LogType::NN_BOSS);

	// PlayReportSetting
	nn::boss::PlayReportSetting::InitVTable();
	cafeExportRegisterFunc(nn::boss::PlayReportSetting::ctor, "nn_boss", "__ct__Q3_2nn4boss17PlayReportSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::PlayReportSetting::dtor, "nn_boss", "__dt__Q3_2nn4boss17PlayReportSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::PlayReportSetting::Initialize, "nn_boss", "Initialize__Q3_2nn4boss17PlayReportSettingFPvUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::PlayReportSetting::Set, "nn_boss", "Set__Q3_2nn4boss17PlayReportSettingFPCcUi", LogType::NN_BOSS);

	// RawDlTaskSetting
	nn::boss::RawDlTaskSetting::InitVTable();
	cafeExportRegisterFunc(nn::boss::RawDlTaskSetting::ctor, "nn_boss", "__ct__Q3_2nn4boss16RawDlTaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::RawDlTaskSetting::dtor, "nn_boss", "__dt__Q3_2nn4boss16RawDlTaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::RawDlTaskSetting::Initialize, "nn_boss", "Initialize__Q3_2nn4boss16RawDlTaskSettingFPCcbT2N21", LogType::NN_BOSS);

	// NetTaskSetting
	nn::boss::NetTaskSetting::InitVTable();
	cafeExportRegisterFunc(nn::boss::NetTaskSetting::ctor, "nn_boss", "__ct__Q3_2nn4boss14NetTaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NetTaskSetting::dtor, "nn_boss", "__dt__Q3_2nn4boss14NetTaskSettingFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NetTaskSetting::SetServiceToken, "nn_boss", "SetServiceToken__Q3_2nn4boss14NetTaskSettingFPCUc", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NetTaskSetting::AddInternalCaCert, "nn_boss", "AddInternalCaCert__Q3_2nn4boss14NetTaskSettingFSc", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NetTaskSetting::SetInternalClientCert, "nn_boss", "SetInternalClientCert__Q3_2nn4boss14NetTaskSettingFSc", LogType::NN_BOSS);

	// Title
	nn::boss::Title::InitVTable();
	cafeExportRegisterFunc(nn::boss::Title::ctor, "nn_boss", "__ct__Q3_2nn4boss5TitleFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Title::dtor, "nn_boss", "__dt__Q3_2nn4boss5TitleFv", LogType::NN_BOSS);
	// cafeExportMakeWrapper<nn::boss::Title::SetNewArrivalFlagOff>("nn_boss", "SetNewArrivalFlagOff__Q3_2nn4boss5TitleFv"); SMM bookmarks

	// TitleId
	cafeExportRegisterFunc(nn::boss::TitleId::ctor1, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TitleId::ctor2, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFUL", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TitleId::ctor3, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFRCQ3_2nn4boss7TitleID", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TitleId::operator_ne, "nn_boss", "__ne__Q3_2nn4boss7TitleIDCFRCQ3_2nn4boss7TitleID", LogType::NN_BOSS);

	// DataName
	cafeExportRegisterFunc(nn::boss::DataName::ctor, "nn_boss", "__ct__Q3_2nn4boss8DataNameFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::DataName::operator_const_char, "nn_boss", "__opPCc__Q3_2nn4boss8DataNameCFv", LogType::NN_BOSS);

	// DirectoryName
	cafeExportRegisterFunc(nn::boss::DirectoryName::ctor, "nn_boss", "__ct__Q3_2nn4boss13DirectoryNameFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::DirectoryName::operator_const_char, "nn_boss", "__opPCc__Q3_2nn4boss13DirectoryNameCFv", LogType::NN_BOSS);

	// Account
	nn::boss::BossAccount::InitVTable();
	cafeExportRegisterFunc(nn::boss::BossAccount::ctor, "nn_boss", "__ct__Q3_2nn4boss7AccountFUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::BossAccount::dtor, "nn_boss", "__dt__Q3_2nn4boss7AccountFv", LogType::NN_BOSS);

	// AlmightyTask
	nn::boss::AlmightyTask::InitVTable();
	cafeExportRegisterFunc(nn::boss::AlmightyTask::ctor, "nn_boss", "__ct__Q3_2nn4boss12AlmightyTaskFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::AlmightyTask::Initialize, "nn_boss", "Initialize__Q3_2nn4boss12AlmightyTaskFQ3_2nn4boss7TitleIDPCcUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::AlmightyTask::dtor, "nn_boss", "__dt__Q3_2nn4boss12AlmightyTaskFv", LogType::NN_BOSS);

	// Storage
	nn::boss::Storage::InitVTable();
	cafeExportRegisterFunc(nn::boss::Storage::ctor1, "nn_boss", "__ct__Q3_2nn4boss7StorageFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Storage::dtor, "nn_boss", "__dt__Q3_2nn4boss7StorageFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Storage::Finalize, "nn_boss", "Finalize__Q3_2nn4boss7StorageFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Storage::Exist, "nn_boss", "Exist__Q3_2nn4boss7StorageCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Storage::GetDataList, "nn_boss", "GetDataList__Q3_2nn4boss7StorageCFPQ3_2nn4boss8DataNameUiPUiT2", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Storage::Initialize, "nn_boss", "Initialize__Q3_2nn4boss7StorageFPCcUiQ3_2nn4boss11StorageKind", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::Storage::Initialize2, "nn_boss", "Initialize__Q3_2nn4boss7StorageFPCcQ3_2nn4boss11StorageKind", LogType::NN_BOSS);

	// AlmightyStorage
	nn::boss::AlmightyStorage::InitVTable();
	cafeExportRegisterFunc(nn::boss::AlmightyStorage::ctor, "nn_boss", "__ct__Q3_2nn4boss15AlmightyStorageFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::AlmightyStorage::Initialize, "nn_boss", "Initialize__Q3_2nn4boss15AlmightyStorageFQ3_2nn4boss7TitleIDPCcUiQ3_2nn4boss11StorageKind", LogType::NN_BOSS);

	// NsData
	nn::boss::NsData::InitVTable();
	cafeExportRegisterFunc(nn::boss::NsData::ctor, "nn_boss", "__ct__Q3_2nn4boss6NsDataFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::dtor, "nn_boss", "__dt__Q3_2nn4boss6NsDataFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::Initialize, "nn_boss", "Initialize__Q3_2nn4boss6NsDataFRCQ3_2nn4boss7StoragePCc", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::DeleteRealFileWithHistory, "nn_boss", "DeleteRealFileWithHistory__Q3_2nn4boss6NsDataFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::Exist, "nn_boss", "Exist__Q3_2nn4boss6NsDataCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::GetSize, "nn_boss", "GetSize__Q3_2nn4boss6NsDataCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::GetCreatedTime, "nn_boss", "GetCreatedTime__Q3_2nn4boss6NsDataCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::Read, "nn_boss", "Read__Q3_2nn4boss6NsDataFPvUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::ReadWithSizeOut, "nn_boss", "Read__Q3_2nn4boss6NsDataFPLPvUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::Seek, "nn_boss", "Seek__Q3_2nn4boss6NsDataFLQ3_2nn4boss12PositionBase", LogType::NN_BOSS);
}
