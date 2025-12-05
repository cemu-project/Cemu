#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/OS/libs/nn_client_service.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"
#include "Cafe/OS/libs/nn_ac/nn_ac.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/IOSU/legacy/iosu_act.h"
#include "config/ActiveSettings.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/Filesystem/fsc.h"
#include "Common/CafeString.h"
#include "Cafe/IOSU/nn/boss/boss_common.h"

namespace nn
{
namespace boss
{
	IPCServiceClient s_ipcClient;

	static constexpr BossResult resultSuccess = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
	static constexpr BossResult resultInvalidParam = BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780);
	static constexpr BossResult resultUknA0220300 = BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_BOSS, 0x20300);

	SysAllocator<coreinit::OSMutex> g_mutex;
	sint32 g_initCounter = 0;
	bool g_isInitialized = false;

	#define DTOR_WRAPPER(__TYPE) RPLLoader_MakePPCCallable([](PPCInterpreter_t* hCPU) { dtor(MEMPTR<__TYPE>(hCPU->gpr[3]), hCPU->gpr[4]); osLib_returnFromFunction(hCPU, 0); })

	SysAllocator<uint8, 0x10000, 64> s_ipcBuffer;

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

	BossResult Initialize() // Initialize__Q2_2nn4bossFv
	{
		coreinit::OSLockMutex(&g_mutex);
		BossResult result = 0;
		if(g_initCounter == 0)
		{
			g_isInitialized = true;
			s_ipcClient.Initialize("/dev/boss", s_ipcBuffer.GetPtr(), s_ipcBuffer.GetByteSize());
			result = resultSuccess;
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
			s_ipcClient.Shutdown();
			g_isInitialized = false;
		}
		g_initCounter--;
		coreinit::OSUnlockMutex(&g_mutex);
	}

	uint32 GetBossState(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebugOnce(LogType::Force, "nn_boss.GetBossState() - stub");
		return 7;
	}

	/* TitleId */

	bool TitleId::IsValid(TitleId* _thisptr)
	{
		return _thisptr->u64 != 0;
	}

	TitleId* TitleId::ctorDefault(TitleId* _thisptr)
	{
		if (!_thisptr)
			_thisptr = boss_new<TitleId>();
		_thisptr->u64 = 0;
		return _thisptr;
	}

	TitleId* TitleId::ctorFromTitleId(TitleId* _thisptr, uint64 titleId) // __ct__Q3_2nn4boss7TitleIDFUL
	{
		if (!_thisptr)
			_thisptr = boss_new<TitleId>();
		_thisptr->u64 = titleId;
		return _thisptr;
	}

	TitleId* TitleId::ctorCopy(TitleId* _thisptr, TitleId* titleId) // __ct__Q3_2nn4boss7TitleIDFRCQ3_2nn4boss7TitleID
	{
		if (!_thisptr)
			_thisptr = boss_new<TitleId>();
		_thisptr->u64 = titleId->u64;
		return _thisptr;
	}

	bool TitleId::operator_ne(TitleId* _thisptr, TitleId* titleId)
	{
		return _thisptr->u64 != titleId->u64;
	}
	/* TaskId */

	TaskId* TaskId::ctorDefault(TaskId* _thisptr)
	{
		if (!_thisptr)
			_thisptr = boss_new<TaskId>();
		_thisptr->id.data[0] = 0;
		return _thisptr;
	}

	TaskId* TaskId::ctorFromString(TaskId* _thisptr, const char* taskId)
	{
		if (!_thisptr)
			_thisptr = boss_new<TaskId>();
		_thisptr->id.assign(taskId);
		return _thisptr;
	}

	/* Title */
	
	Title* Title::ctor(Title* _this)
	{
		if (!_this)
			_this = boss_new<Title>();
		*_this = {};
		_this->vTablePtr = s_titleVTable;
		return _this;
	}

	void Title::dtor(Title* _this, uint32 options)
	{
		if (_this && (options & 1))
			boss_delete(_this);
	}

	void Title::InitVTable()
	{
		s_titleVTable->rtti.ptr = nullptr; // todo
		s_titleVTable->dtor.ptr = DTOR_WRAPPER(Title);
	}

	/* DirectoryName */

	DirectoryName* DirectoryName::ctor(DirectoryName* _thisptr)
	{
		if (!_thisptr)
			_thisptr = boss_new<DirectoryName>();
		_thisptr->name2.ClearAllBytes();
		return _thisptr;
	}

	const char* DirectoryName::operator_const_char(DirectoryName* _thisptr)
	{
		return _thisptr->name2.c_str();
	}

	/* BossAccount */

	BossAccount* BossAccount::ctor(BossAccount* _this, uint32 accountId)
	{
		if (!_this)
			_this = boss_new<BossAccount>();
		_this->accountId = accountId;
		_this->vTablePtr = s_VTable;
		return _this;
	}

	void BossAccount::dtor(BossAccount* _this, uint32 options)
	{
		if(_this && options & 1)
			boss_delete(_this);
	}

	void BossAccount::InitVTable()
	{
		s_VTable->rtti.ptr = nullptr; // todo
		s_VTable->dtor.ptr = DTOR_WRAPPER(BossAccount);
	}

	/* TaskSetting */

	TaskSetting* TaskSetting::ctor(TaskSetting* _thisptr)
	{
		if(!_thisptr)
			_thisptr = boss_new<TaskSetting>();
		_thisptr->vTablePtr = s_VTable;
		InitializeSetting(_thisptr);
		return _thisptr;
	}

	void TaskSetting::dtor(TaskSetting* _this, uint32 options)
	{
		if(options & 1)
			boss_delete(_this);
	}

	bool TaskSetting::IsPrivileged(TaskSetting* _thisptr)
	{
		const TaskType taskType = _thisptr->taskType;
		return taskType == TaskType::RawDlTaskSetting_1 || taskType == TaskType::RawDlTaskSetting_9 || taskType == TaskType::PlayLogUploadTaskSetting;
	}

	void TaskSetting::InitializeSetting(TaskSetting* _thisptr)
	{
		memset(_thisptr, 0x00, 0x1000);
		_thisptr->persistentId = 0;
		_thisptr->ukn08 = 0;
		_thisptr->ukn0C = 0;
		_thisptr->priority = 125;
		_thisptr->intervalB = 28800;
		_thisptr->lifeTime.high = 0;
		_thisptr->lifeTime.low = 0x76A700;
	}

	void TaskSetting::InitVTable()
	{
		s_VTable->rtti.ptr = nullptr; // todo
		s_VTable->dtor.ptr = DTOR_WRAPPER(TaskSetting);
		s_VTable->RegisterPreprocess.ptr = nullptr; // todo
		s_VTable->unk1.ptr = nullptr; // todo
	}

	/* NetTaskSetting */

	NetTaskSetting* NetTaskSetting::ctor(NetTaskSetting* _thisptr)
	{
		if (!_thisptr)
			_thisptr = boss_new<NetTaskSetting>();
		TaskSetting::ctor(_thisptr);
		_thisptr->httpTimeout = 120;
		_thisptr->vTablePtr = s_VTable;
		return _thisptr;
	}

	BossResult NetTaskSetting::AddCaCert(NetTaskSetting* _thisptr, const char* name)
	{
		if(name == nullptr || strnlen(name, 0x80) == 0x80)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddCaCert: name size is invalid");
			return resultInvalidParam;
		}

		cemu_assert_unimplemented();

		return 0xA0220D00;
	}

	BossResult NetTaskSetting::SetServiceToken(NetTaskSetting* _thisptr, const uint8* serviceToken) // input is uint8[512]?
	{
		memcpy(_thisptr->serviceToken.data, serviceToken, _thisptr->serviceToken.Size());
		return resultSuccess;
	}

	BossResult NetTaskSetting::AddInternalCaCert(NetTaskSetting* _thisptr, char certId)
	{
		for(int i = 0; i < 3; i++)
		{
			if(_thisptr->internalCaCert[i] == 0)
			{
				_thisptr->internalCaCert[i] = (uint8)certId;
				return resultSuccess;
			}
		}
		cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddInternalCaCert: Cannot store more than 3 certificates");
		return 0xA0220D00;
	}

	void NetTaskSetting::SetInternalClientCert(NetTaskSetting* _thisptr, char certId)
	{
		_thisptr->internalClientCert = (uint8)certId;
	}

	void NetTaskSetting::InitVTable()
	{
		s_VTable->rtti.ptr = nullptr; // todo
		s_VTable->dtor.ptr = DTOR_WRAPPER(NetTaskSetting);
		s_VTable->RegisterPreprocess.ptr = nullptr; // todo
		s_VTable->unk1.ptr = nullptr; // todo
	}

	/* NbdlTaskSetting */

	NbdlTaskSetting* NbdlTaskSetting::ctor(NbdlTaskSetting* _thisptr)
	{
		if (!_thisptr)
			_thisptr = boss_new<NbdlTaskSetting>();
		NetTaskSetting::ctor(_thisptr);
		_thisptr->vTablePtr = s_VTable;
		return _thisptr;
	}

	BossResult NbdlTaskSetting::Initialize(NbdlTaskSetting* _thisptr, const char* bossCode, uint64 directorySizeLimit, const char* bossDirectory) // Initialize__Q3_2nn4boss15NbdlTaskSettingFPCcLT1
	{
		if (!bossCode || !_thisptr->nbdl.bossCode.CanHoldString(bossCode))
			return resultInvalidParam;
		if (bossDirectory && !_thisptr->nbdl.bossDirectory.CanHoldString(bossDirectory))
			return resultInvalidParam; // directory is optional, but if a string is passed it must fit
		_thisptr->nbdl.bossCode.assign(bossCode);
		if (bossDirectory)
			_thisptr->nbdl.bossDirectory.assign(bossDirectory);
		_thisptr->nbdl.directorySizeLimitHigh = (uint32be)(directorySizeLimit >> 32);
		_thisptr->nbdl.directorySizeLimitLow = (uint32be)(directorySizeLimit & 0xFFFFFFFF);
		_thisptr->taskType = TaskType::NbdlTaskSetting;
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
	}

	BossResult NbdlTaskSetting::SetFileName(NbdlTaskSetting* _thisptr, const char* fileName)
	{
		if (!fileName || !_thisptr->nbdl.fileName.CanHoldString(fileName))
			return resultInvalidParam;
		_thisptr->nbdl.fileName.assign(fileName);
		return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
	}

	void NbdlTaskSetting::InitVTable()
	{
		s_VTable->rtti.ptr = nullptr; // todo
		s_VTable->dtor.ptr = DTOR_WRAPPER(NbdlTaskSetting);
		s_VTable->RegisterPreprocess.ptr = nullptr; // todo
		s_VTable->unk1.ptr = nullptr; // todo
		s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
	}

	/* RawUlTaskSetting */

	RawUlTaskSetting* RawUlTaskSetting::ctor(RawUlTaskSetting* _thisptr)
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

	void RawUlTaskSetting::dtor(RawUlTaskSetting* _this, uint32 options)
	{
		cemuLog_logDebug(LogType::Force, "nn::boss::RawUlTaskSetting::dtor() is todo");
	}

	void RawUlTaskSetting::InitVTable()
	{
		s_VTable->rtti.ptr = nullptr; // todo
		s_VTable->dtor.ptr = DTOR_WRAPPER(RawUlTaskSetting);
		s_VTable->RegisterPreprocess.ptr = nullptr; // todo
		s_VTable->unk1.ptr = nullptr; // todo
		s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
	}

	/* RawDlTaskSetting */

	RawDlTaskSetting* RawDlTaskSetting::ctor(RawDlTaskSetting* _thisptr)
	{
		if (!_thisptr)
			_thisptr = boss_new<RawDlTaskSetting>();
		NetTaskSetting::ctor(_thisptr);
		_thisptr->vTablePtr = s_VTable;
		return _thisptr;
	}

	BossResult RawDlTaskSetting::Initialize(RawDlTaskSetting* _thisptr, const char* url, bool newArrival, bool led, const char* fileName, const char* bossDirectory)
	{
		if (!url || !_thisptr->url.CanHoldString(url))
			return resultInvalidParam;
		cemuLog_logDebug(LogType::Force, "RawDlTaskSetting::Initialize url: {}", url);
		if (fileName && !_thisptr->rawDl.fileName.CanHoldString(fileName))
			return resultInvalidParam; // fileName is optional, but if a string is passed it must fit
		if (!bossDirectory || !_thisptr->rawDl.bossDirectory.CanHoldString(bossDirectory))
			return resultInvalidParam;

		_thisptr->url.assign(url);
		_thisptr->rawDl.fileName.assign(fileName ? fileName : "rawcontent.dat");
		_thisptr->rawDl.bossDirectory.assign(bossDirectory);
		_thisptr->rawDl.newArrival = newArrival;
		_thisptr->rawDl.led = led;
		_thisptr->taskType = TaskType::RawDlTaskSetting_3;
		return resultSuccess;
	}

	void RawDlTaskSetting::InitVTable()
	{
		s_VTable->rtti.ptr = nullptr; // todo
		s_VTable->dtor.ptr = DTOR_WRAPPER(RawDlTaskSetting);
		s_VTable->RegisterPreprocess.ptr = nullptr; // todo
		s_VTable->unk1.ptr = nullptr; // todo
		s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
	}

	/* PlayReportSetting */

	PlayReportSetting* PlayReportSetting::ctor(PlayReportSetting* _this)
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

	void PlayReportSetting::dtor(PlayReportSetting* _this, uint32 options)
	{
		RawUlTaskSetting::dtor(_this, 0);
		if(options&1)
			boss_delete(_this->ukn1210_ptr.GetPtr());
	}

	void PlayReportSetting::Initialize(PlayReportSetting* _this, uint8* ptr, uint32 size)
	{
		if (!ptr || size == 0 || size > 0x19000)
		{
			cemuLog_logDebug(LogType::Force, "nn::boss::PlayReportSetting::Initialize: invalid parameter");
			return;
		}

		*ptr = 0;

		_this->taskType = TaskType::PlayReportSetting;
		_this->mode |= 3;
		_this->rawUl.optionValue |= 2;
		_this->permission |= 0xA;

		_this->ukn1210_ptr = ptr;
		_this->ukn1214_size = size;
		_this->ukPlay3 = 0;
		_this->ukPlay4 = 0;

		_this->AddInternalCaCert(_this, 102);
		_this->SetInternalClientCert(_this, 1);

		// todo - incomplete
	}

	bool PlayReportSetting::Set(PlayReportSetting* _this, const char* keyname, uint32 value)
	{
		// TODO
		return true;
	}

	void PlayReportSetting::InitVTable()
	{
		s_VTable->rtti.ptr = nullptr; // todo
		s_VTable->dtor.ptr = DTOR_WRAPPER(PlayReportSetting);
		s_VTable->RegisterPreprocess.ptr = nullptr; // todo
		s_VTable->unk1.ptr = nullptr; // todo
		s_VTable->rttiNetTaskSetting.ptr = nullptr; // todo
	}

	/* Task */

	struct Task
	{
		struct VTableTask
		{
			VTableEntry rtti;
			VTableEntry dtor;
		};
		static inline SysAllocator<VTableTask> s_vTable;

		uint32be persistentId; // 0x00
		uint32be uk2; // 0x04
		TaskId taskId; // 0x08
		TitleId titleId; // 0x10
		MEMPTR<VTableTask> vTablePtr; // 0x18
		uint32be padding; // 0x1C

		static BossResult Initialize1(Task* _thisptr, const char* taskId, uint32 persistentId) // Initialize__Q3_2nn4boss4TaskFPCcUi
		{
			if (!taskId || !_thisptr->taskId.id.CanHoldString(taskId))
				return resultInvalidParam;
			_thisptr->persistentId = persistentId;
			_thisptr->taskId.id.assign(taskId);
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
		}

		static BossResult Initialize2(Task* _thisptr, uint8 slot, const char* taskId) // Initialize__Q3_2nn4boss4TaskFUcPCc
		{
			const uint32 persistentId = slot == 0 ? 0 : act::GetPersistentIdEx(slot);
			return Initialize1(_thisptr, taskId, persistentId);
		}

		static BossResult Initialize3(Task* _thisptr, const char* taskId) // Initialize__Q3_2nn4boss4TaskFPCc
		{
			return Initialize1(_thisptr, taskId, 0);
		}

		static Task* ctor2(Task* _thisptr, const char* taskId, uint32 persistentId) // __ct__Q3_2nn4boss4TaskFPCcUi
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->persistentId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctorDefault(&_thisptr->taskId);
			TitleId::ctorFromTitleId(&_thisptr->titleId, 0);
			auto r = Initialize1(_thisptr, taskId, persistentId);
			cemu_assert_debug(NN_RESULT_IS_SUCCESS(r));
			return _thisptr;
		}

		static Task* ctor1(Task* _thisptr, uint8 slot, const char* taskId) // __ct__Q3_2nn4boss4TaskFUcPCc
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->persistentId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctorDefault(&_thisptr->taskId);
			TitleId::ctorFromTitleId(&_thisptr->titleId, 0);
			auto r = Initialize2(_thisptr, slot, taskId);
			cemu_assert_debug(NN_RESULT_IS_SUCCESS(r));
			return _thisptr;
		}

		static Task* ctor3(Task* _thisptr, const char* taskId) // __ct__Q3_2nn4boss4TaskFPCc
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->persistentId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctorDefault(&_thisptr->taskId);
			TitleId::ctorFromTitleId(&_thisptr->titleId, 0);
			auto r = Initialize3(_thisptr, taskId);
			cemu_assert_debug(NN_RESULT_IS_SUCCESS(r));
			return _thisptr;
		}

		static Task* ctor4(Task* _thisptr) // __ct__Q3_2nn4boss4TaskFv
		{
			if (!_thisptr)
				_thisptr = boss_new<Task>();
			_thisptr->persistentId = 0;
			_thisptr->vTablePtr = s_vTable;
			TaskId::ctorDefault(&_thisptr->taskId);
			TitleId::ctorFromTitleId(&_thisptr->titleId, 0);
			memset(&_thisptr->taskId, 0x00, sizeof(TaskId));
			return _thisptr;
		}

		static void dtor(Task* _this, uint32 options) // __dt__Q3_2nn4boss4TaskFv
		{
			// todo - Task::Finalize
			if(options & 1)
				boss_delete(_this);
		}

		static BossResult Run(Task* _thisptr, bool isForegroundRun)
		{
			uint8be isConnected = 0;
			nn_ac::IsApplicationConnected(&isConnected);
			if (isForegroundRun && !isConnected)
			{
				cemuLog_logDebug(LogType::Force, "nn::boss::Task::Run: Application is not connected, returning error");
				return 0xA0223A00;
			}
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskRun));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			serviceCall.WriteParam<uint8>(isForegroundRun ? 1 : 0);
			return serviceCall.Submit();
		}

		static BossResult StartScheduling(Task* _thisptr, uint8 executeImmediately)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskStartScheduling));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			serviceCall.WriteParam<uint8be>(executeImmediately);
			return serviceCall.Submit();
		}

		static nnResult StopScheduling(Task* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskStopScheduling));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			return serviceCall.Submit();
		}

		static bool IsRegistered(Task* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskIsRegistered));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return false;
			return serviceCall.ReadResponse<uint8be>() != 0;
		}

		static bool Wait(Task* _thisptr, uint32 timeout, TaskWaitState waitState) // Wait__Q3_2nn4boss4TaskFUiQ3_2nn4boss13TaskWaitState
		{
			static_assert(sizeof(TaskSettingCore) == 0xC00);
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskWaitA));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			serviceCall.WriteParam<betype<TaskWaitState>>(waitState);
			serviceCall.WriteParam<uint32be>(timeout);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return false;
			uint8 isNotTimeout = serviceCall.ReadResponse<uint8be>();
			return isNotTimeout;
		}

		static BossResult RegisterForImmediateRun(Task* _thisptr, TaskSetting* settings) // RegisterForImmediateRun__Q3_2nn4boss4TaskFRCQ3_2nn4boss11TaskSetting
		{
			static_assert(sizeof(TaskSettingCore) == 0xC00);
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskRegisterForImmediateRunA));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			serviceCall.WriteParamBuffer(settings, sizeof(TaskSettingCore));
			return serviceCall.Submit();
		}

		static BossResult Unregister(Task* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskUnregister));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			return serviceCall.Submit();
		}

		static BossResult Register(Task* _thisptr, TaskSetting* settings)
		{
			static_assert(sizeof(TaskSettingCore) == 0xC00);
			if (!settings)
			{
				cemuLog_logDebug(LogType::Force, "nn_boss_Task_Register - crash workaround (fix me)"); // settings should never be zero
				return 0;
			}
			// todo - missing vcall which leads to nn::boss::PlayReportSetting::RegisterPreprocess (and other functions?) being called?
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskRegisterA));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			serviceCall.WriteParamBuffer(settings, sizeof(TaskSettingCore));
			return serviceCall.Submit();
		}

		static TaskTurnState GetTurnState(Task* _thisptr, uint32be* executionCounter)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskGetTurnState));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return (TaskTurnState)0;
			uint32 executionCount = serviceCall.ReadResponse<uint32be>();
			if (executionCounter)
				*executionCounter = executionCount;
			return serviceCall.ReadResponse<betype<TaskTurnState>>();
		}

		static uint64 GetContentLength(Task* _thisptr, uint32be* executionCounter)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskGetContentLength));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return 0;
			uint32 executionCount = serviceCall.ReadResponse<uint32be>();
			if (executionCounter)
				*executionCounter = executionCount;
			return serviceCall.ReadResponse<uint64be>();
		}

		static uint64 GetProcessedLength(Task* _thisptr, uint32be* executionCounter)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskGetProcessedLength));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return 0;
			uint32 executionCount = serviceCall.ReadResponse<uint32be>();
			if (executionCounter)
				*executionCounter = executionCount;
			return serviceCall.ReadResponse<uint64be>();
		}

		static uint32 GetHttpStatusCode(Task* _thisptr, uint32be* executionCounter)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::TaskGetHttpStatusCodeA));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<TaskId>(_thisptr->taskId);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return 0;
			uint32 executionCount = serviceCall.ReadResponse<uint32be>();
			if (executionCounter)
				*executionCounter = executionCount;
			return serviceCall.ReadResponse<uint32be>();
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
				return resultInvalidParam;
			_thisptr->persistentId = accountId;
			_thisptr->titleId.u64 = titleId->u64;
			_thisptr->taskId.id.assign(taskId);
			return resultSuccess;
		}

		static void InitVTable()
		{
			s_VTable->rtti.ptr = nullptr; // todo
			s_VTable->dtor.ptr = DTOR_WRAPPER(AlmightyTask);
			s_VTable->rttiTask.ptr = nullptr; // todo
		}
	};
	static_assert(sizeof(AlmightyTask) == 0x20);

	DataName* DataName::ctor(DataName* _this)
	{
		if(!_this)
			_this = boss_new<DataName>();
		_this->name.ClearAllBytes();
		return _this;
	}

	const char* DataName::operator_const_char(DataName* _this)
	{
		return _this->name.c_str();
	}

	struct BossStorageFadEntry
	{
		CafeString<32> name;
		uint32be dataId;
		uint32 ukn24;
		uint32 ukn28;
		uint32 ukn2C;
		uint32 ukn30;
		uint32be timestampRelated; // unsure
	};
	static_assert(sizeof(BossStorageFadEntry) == 0x38);

	struct Storage
	{
		static_assert(sizeof(DirectoryName) == 8);
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

		/* +0x00 */ uint32be persistentId;
		/* +0x04 */ uint32be storageKind;
		/* +0x08 */ uint8 ukn08Array[3];
		/* +0x0B */ DirectoryName storageName;
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
			Finalize(_this);
			if(options & 1)
				boss_delete(_this);
		}

		static BossResult Initialize(Storage* _thisptr, const char* storageName, uint32 accountId, StorageKind type)
		{
			if (!storageName)
				return resultInvalidParam;
			_thisptr->storageKind = type;
			_thisptr->titleId.u64 = 0;
			_thisptr->storageName.name2.assign(storageName);
			_thisptr->persistentId = accountId;
			return resultSuccess;
		}

		static BossResult Initialize2(Storage* _thisptr, const char* dirName, StorageKind type)
		{
			return Initialize(_thisptr, dirName, 0, type);
		}

		static void Finalize(Storage* _this)
		{
			memset(_this, 0, sizeof(Storage)); // todo - not all fields might be cleared
		}

		static bool Exist(nn::boss::Storage* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::StorageExist));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<DirectoryName>(_thisptr->storageName);
			serviceCall.WriteParam<betype<StorageKind>>(_thisptr->storageKind);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return 0;
			return serviceCall.ReadResponse<uint8be>();
		}

		static nnResult GetDataList(nn::boss::Storage* _thisptr, DataName* dataList, sint32 maxEntries, uint32be* outputEntryCount, uint32 startIndex) // GetDataList__Q3_2nn4boss7StorageCFPQ3_2nn4boss8DataNameUiPUiT2
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::StorageGetDataList));
			serviceCall.WriteParam<uint32be>(_thisptr->persistentId);
			serviceCall.WriteParam<DirectoryName>(_thisptr->storageName);
			serviceCall.WriteParam<betype<StorageKind>>(_thisptr->storageKind);
			serviceCall.WriteResponseBuffer(dataList, sizeof(DataName) * maxEntries);
			serviceCall.WriteParam<uint32be>(startIndex);
			nnResult r = serviceCall.Submit();
			uint32be outputCount = serviceCall.ReadResponse<uint32be>();
			bool isSuccessByte = serviceCall.ReadResponse<uint8be>() != 0;
			if (!isSuccessByte)
				return resultUknA0220300;
			if (outputEntryCount)
				*outputEntryCount = outputCount;
			return r;
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
			if (!_thisptr)
				_thisptr = boss_new<AlmightyStorage>();
			Storage::ctor1(_thisptr);
			_thisptr->vTablePtr = s_VTable;
			return _thisptr;
		}

		static uint32 Initialize(AlmightyStorage* _thisptr, TitleId* titleId, const char* storageName, uint32 accountId, StorageKind storageKind)
		{
			if (!_thisptr)
				return resultInvalidParam;

			_thisptr->persistentId = accountId;
			_thisptr->storageKind = storageKind;
			_thisptr->titleId.u64 = titleId->u64;

			_thisptr->storageName.name2.assign(storageName);

			return resultSuccess;
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

		/* +0x00 */ DataName name;
		/* +0x20 */ Storage storage;
		/* +0x48 */ uint64be currentSeek;
		/* +0x50 */ MEMPTR<void> vTablePtr;
		/* +0x54 */ uint32 ukn54;

		static NsData* ctor(NsData* _this)
		{
			if (!_this)
				_this = boss_new<NsData>();
			_this->vTablePtr = s_vTable;
			DataName::ctor(&_this->name);
			_this->storage.ctor1(&_this->storage);
			_this->currentSeek = 0;
			return _this;
		}

		static void dtor(NsData* _this, uint32 options) // __dt__Q3_2nn4boss6NsDataFv
		{
			_this->storage.dtor(&_this->storage, 0);
			// todo
			if(options & 1)
				boss_delete(_this);
		}

		static BossResult Initialize(NsData* _this, nn::boss::Storage* storage, const char* dataName)
		{
			if(dataName == nullptr)
			{
				if (storage->storageKind != 1)
				{
					return resultInvalidParam;
				}
			}

			_this->storage.persistentId = storage->persistentId;
			_this->storage.storageKind = storage->storageKind;
			memcpy(_this->storage.ukn08Array, storage->ukn08Array, 3);
			_this->storage.storageName = storage->storageName;
			_this->storage.titleId = storage->titleId;
			_this->storage = *storage;

			if (dataName != nullptr || storage->storageKind != 1)
				_this->name.name.assign(dataName);
			else
				_this->name.name.assign("rawcontent.dat");
			_this->currentSeek = 0;

			return resultSuccess;
		}

		static std::string _GetPath(NsData* nsData)
		{
			uint32 accountId = nsData->storage.persistentId;
			if (accountId == 0)
				accountId = iosuAct_getAccountIdOfCurrentAccount();

			uint64 title_id = nsData->storage.titleId.u64;
			if (title_id == 0)
				title_id = CafeSystem::GetForegroundTitleId();

			fs::path path = fmt::format("cemuBossStorage/{:08x}/{:08x}/user/{:08x}", (uint32)(title_id >> 32), (uint32)(title_id & 0xFFFFFFFF), accountId);
			path /= nsData->storage.storageName.name2.c_str();
			path /= nsData->name.c_str();
			return path.string();
		}

		static BossResult DeleteRealFile(NsData* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::NsDataDeleteFile));
			serviceCall.WriteParam<uint32be>(_thisptr->storage.persistentId);
			serviceCall.WriteParam<DirectoryName>(_thisptr->storage.storageName);
			serviceCall.WriteParam<betype<StorageKind>>(_thisptr->storage.storageKind);
			serviceCall.WriteParam<DataName>(_thisptr->name);
			return serviceCall.Submit();
		}

		static BossResult DeleteRealFileWithHistory(NsData* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::NsDataDeleteFileWithHistory));
			serviceCall.WriteParam<uint32be>(_thisptr->storage.persistentId);
			serviceCall.WriteParam<DirectoryName>(_thisptr->storage.storageName);
			serviceCall.WriteParam<betype<StorageKind>>(_thisptr->storage.storageKind);
			serviceCall.WriteParam<DataName>(_thisptr->name);
			return serviceCall.Submit();
		}

		static uint32 Exist(NsData* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::NsDataExist));
			serviceCall.WriteParam<uint32be>(_thisptr->storage.persistentId);
			serviceCall.WriteParam<DirectoryName>(_thisptr->storage.storageName);
			serviceCall.WriteParam<betype<StorageKind>>(_thisptr->storage.storageKind);
			serviceCall.WriteParam<DataName>(_thisptr->name);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return 0;
			return serviceCall.ReadResponse<uint8be>();
		}

		static uint64 GetSize(NsData* _thisptr)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::NsDataGetSize));
			serviceCall.WriteParam<uint32be>(_thisptr->storage.persistentId);
			serviceCall.WriteParam<DirectoryName>(_thisptr->storage.storageName);
			serviceCall.WriteParam<betype<StorageKind>>(_thisptr->storage.storageKind);
			serviceCall.WriteParam<DataName>(_thisptr->name);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return 0;
			return serviceCall.ReadResponse<uint64be>();
		}

		static uint64 GetCreatedTime(NsData* nsData)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss.NsData_GetCreatedTime() not implemented. Returning 0");
			uint64 createdTime = 0;
			// cmdId 0x97
			// probably uses FS stat query
			return createdTime;
		}

		static sint32 ReadWithSizeOut(NsData* _thisptr, uint64be* sizeOut, uint8* buffer, sint32 length)
		{
			IPCServiceClient::IPCServiceCall serviceCall = s_ipcClient.Begin(0, stdx::to_underlying(BossCommandId::NsDataRead));
			serviceCall.WriteParam<uint32be>(_thisptr->storage.persistentId);
			serviceCall.WriteParam<DirectoryName>(_thisptr->storage.storageName);
			serviceCall.WriteParam<betype<StorageKind>>(_thisptr->storage.storageKind);
			serviceCall.WriteParam<DataName>(_thisptr->name);
			serviceCall.WriteResponseBuffer(buffer, length);
			uint64 readOffset = _thisptr->currentSeek;
			serviceCall.WriteParam<uint64be>(readOffset);
			nnResult r = serviceCall.Submit();
			if (NN_RESULT_IS_FAILURE(r))
				return r;
			uint64 numReadBytes = serviceCall.ReadResponse<uint64be>();
			_thisptr->currentSeek += numReadBytes;
			cemu_assert(sizeOut);
			cemu_assert(numReadBytes <= length);
			*sizeOut = numReadBytes;
			return r;
		}

		static sint32 Read(NsData* _thisptr, uint8* buffer, sint32 length)
		{
			uint64be bytesRead = 0;
			return ReadWithSizeOut(_thisptr, &bytesRead, buffer, length);
		}

		enum class NsDataSeekMode
		{
			Beginning = 0,
			Relative = 1,
			Ending = 2
		};

		static BossResult Seek(NsData* _thisptr, sint64 seekPos, NsDataSeekMode mode)
		{
			if (mode == NsDataSeekMode::Beginning)
			{
				_thisptr->currentSeek = seekPos;
			}
			else if (mode == NsDataSeekMode::Relative)
			{
				_thisptr->currentSeek += seekPos;
			}
			else if (mode == NsDataSeekMode::Ending)
			{
				uint64 fileSize = GetSize(_thisptr);
				_thisptr->currentSeek = fileSize + seekPos;
			}
			else
			{
				cemu_assert_unimplemented();
			}
			return resultSuccess;
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

	// taskId
	cafeExportRegisterFunc(nn::boss::TaskId::ctorDefault, "nn_boss", "__ct__Q3_2nn4boss6TaskIDFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TaskId::ctorFromString, "nn_boss", "__ct__Q3_2nn4boss6TaskIDFPCc", LogType::NN_BOSS);

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
	cafeExportRegisterFunc(nn::boss::TitleId::ctorDefault, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TitleId::ctorFromTitleId, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFUL", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::TitleId::ctorCopy, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFRCQ3_2nn4boss7TitleID", LogType::NN_BOSS);
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
	cafeExportRegisterFunc(nn::boss::NsData::DeleteRealFile, "nn_boss", "DeleteRealFile__Q3_2nn4boss6NsDataFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::DeleteRealFileWithHistory, "nn_boss", "DeleteRealFileWithHistory__Q3_2nn4boss6NsDataFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::Exist, "nn_boss", "Exist__Q3_2nn4boss6NsDataCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::GetSize, "nn_boss", "GetSize__Q3_2nn4boss6NsDataCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::GetCreatedTime, "nn_boss", "GetCreatedTime__Q3_2nn4boss6NsDataCFv", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::Read, "nn_boss", "Read__Q3_2nn4boss6NsDataFPvUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::ReadWithSizeOut, "nn_boss", "Read__Q3_2nn4boss6NsDataFPLPvUi", LogType::NN_BOSS);
	cafeExportRegisterFunc(nn::boss::NsData::Seek, "nn_boss", "Seek__Q3_2nn4boss6NsDataFLQ3_2nn4boss12PositionBase", LogType::NN_BOSS);
}
