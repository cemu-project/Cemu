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

	void freeMem(void* mem)
	{
		if(mem)
			coreinit::default_MEMFreeToDefaultHeap((uint8*)mem - 8);
	}

	struct TaskSetting_t
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
		uint32be uknExt_vTableProbably; // +0x1000
	};
	static_assert(sizeof(TaskSetting_t) == 0x1004);
	static_assert(offsetof(TaskSetting_t, uknExt_vTableProbably) == 0x1000, "offsetof(TaskSetting_t, uknExt)");

	struct NetTaskSetting_t : TaskSetting_t
	{
		// 0x188 cert1 + 0x188 cert2 + 0x188 cert3
		// 0x190 AddCaCert (3times) char cert[0x80];
		// SetConnectionSetting
		// SetFirstLastModifiedTime
	};
	static_assert(sizeof(NetTaskSetting_t) == 0x1004);

	struct NbdlTaskSetting_t : NetTaskSetting_t
	{
		//char fileName[0x20]; // @0x7F8
	};
	static_assert(sizeof(NbdlTaskSetting_t) == 0x1004);

	struct RawUlTaskSetting_t : NetTaskSetting_t
	{
		static const uint32 kType = 0x12340000;
		uint32be ukRaw1; // 0x1004
		uint32be ukRaw2; // 0x1008
		uint32be ukRaw3; // 0x100C
		uint8 rawSpace[0x200]; // 0x1010
	};
	static_assert(sizeof(RawUlTaskSetting_t) == 0x1210);

	struct PlayReportSetting_t : RawUlTaskSetting_t
	{
		static const uint32 kType = 0x12340001;
		MEMPTR<void*> ukPlay1; // 0x1210
		uint32be ukPlay2; // 0x1214
		uint32be ukPlay3; // 0x1218
		uint32be ukPlay4; // 0x121C
	};
	static_assert(sizeof(PlayReportSetting_t) == 0x1220);

	struct RawDlTaskSetting_t : NetTaskSetting_t
	{
		static const uint32 kType = 0x12340002;
		//char fileName[0x20]; // 0x7F8
	};
	static_assert(sizeof(RawDlTaskSetting_t) == 0x1004);

	struct TitleId_t
	{
		uint64be u64{};
	};
	static_assert(sizeof(TitleId_t) == 8);

	struct TaskId_t
	{
		char id[0x8]{};
	};
	static_assert(sizeof(TaskId_t) == 8);

	struct Title_t
	{
		uint32be accountId{}; // 0x00
		TitleId_t titleId{}; // 0x8
		uint32be someValue = 0x12341000; // 0x10
	};
	static_assert(sizeof(Title_t) == 0x18);

	struct DirectoryName_t
	{
		char name[0x8]{};
	};
	static_assert(sizeof(DirectoryName_t) == 8);

	struct Account_t
	{
		uint32be accountId;
		uint32be uk1; // global struct
	};
	static_assert(sizeof(Account_t) == 8);
	
	struct Task_t
	{
		uint32be accountId; // 0x00
		uint32be uk2; // 0x04
		TaskId_t taskId; // 0x08
		TitleId_t titleId; // 0x10
		uint32be ext; // 0x18
		uint32be padding; // 0x1C
	};
	static_assert(sizeof(Task_t) == 0x20, "sizeof(Task_t)");

	namespace TaskId
	{
		TaskId_t* ctor(TaskId_t* thisptr)
		{
			if(!thisptr)
			{
				// thisptr = new TaskId_t
				assert_dbg();
			}

			if(thisptr)
			{
				thisptr->id[0] = 0;
			}

			return thisptr;
		}
	}

	namespace Account
	{
		Account_t* ctor(Account_t* thisptr, uint32 accountId)
		{
			if (!thisptr)
			{
				// thisptr = new TaskId_t
				assert_dbg();
			}

			thisptr->accountId = accountId;
			thisptr->uk1 = 0x12340010;
			return thisptr;
		}
	}
	
	namespace TitleId
	{
		TitleId_t* ctor(TitleId_t* thisptr, uint64 titleId)
		{
			if (!thisptr)
			{
				// thisptr = new TaskId_t
				assert_dbg();
			}

			if (thisptr)
			{
				thisptr->u64 = titleId;
			}

			return thisptr;
		}

		TitleId_t* ctor(TitleId_t* thisptr)
		{
			return ctor(thisptr, 0);
		}

		bool IsValid(TitleId_t* thisptr)
		{
			return thisptr->u64 != 0;
		}

		TitleId_t* ctor1(TitleId_t* thisptr, uint32 filler, uint64 titleId)
		{
			return ctor(thisptr);
		}

		TitleId_t* ctor2(TitleId_t* thisptr, uint32 filler, uint64 titleId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_TitleId_ctor2(0x{:x})", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			thisptr->u64 = titleId;
			return thisptr;
		}


		TitleId_t* cctor(TitleId_t* thisptr, TitleId_t* titleId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_TitleId_cctor(0x{:x})", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			thisptr->u64 = titleId->u64;

			return thisptr;
		}

		bool operator_ne(TitleId_t* thisptr, TitleId_t* titleId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_TitleId_operator_ne(0x{:x})", MEMPTR(thisptr).GetMPTR());
			return thisptr->u64 != titleId->u64;
		}
	}

	namespace TaskSetting
	{
		bool IsPrivilegedTaskSetting(TaskSetting_t* thisptr)
		{
			const uint16 value = *(uint16*)&thisptr->settings[0x28];
			return value == 1 || value == 9 || value == 5;
		}

		void InitializeSetting(TaskSetting_t* thisptr)
		{
			memset(thisptr, 0x00, sizeof(TaskSetting_t::settings));
			*(uint32*)&thisptr->settings[0x0C] = 0;
			*(uint8*)&thisptr->settings[0x2A] = 0x7D; // timeout?
			*(uint32*)&thisptr->settings[0x30] = 0x7080;
			*(uint32*)&thisptr->settings[0x8] = 0;
			*(uint32*)&thisptr->settings[0x38] = 0;
			*(uint32*)&thisptr->settings[0x3C] = 0x76A700;
			*(uint32*)&thisptr->settings[0] = 0x76A700;
		}

		TaskSetting_t* ctor(TaskSetting_t* thisptr)
		{
			if(!thisptr)
			{
				// thisptr = new TaskSetting_t
				assert_dbg();
			}

			if (thisptr)
			{
				thisptr->uknExt_vTableProbably = 0;
				InitializeSetting(thisptr);
			}

			return thisptr;
		}

		
	}

	namespace NetTaskSetting
	{
		Result AddCaCert(NetTaskSetting_t* thisptr, const char* name)
		{
			if(name == nullptr || strnlen(name, 0x80) == 0x80)
			{
				cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddCaCert: name size is invalid");
				return 0xC0203780;
			}

			cemu_assert_unimplemented();

			return 0xA0220D00;
		}

		NetTaskSetting_t* ctor(NetTaskSetting_t* thisptr)
		{
			if (!thisptr)
			{
				// thisptr = new NetTaskSetting_t
				assert_dbg();
			}

			if (thisptr)
			{
				TaskSetting::ctor(thisptr);
				*(uint32*)&thisptr->settings[0x18C] = 0x78;
				thisptr->uknExt_vTableProbably = 0;
			}

			return thisptr;
		}

		Result SetServiceToken(NetTaskSetting_t* thisptr, const uint8* serviceToken)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_SetServiceToken(0x{:x}, 0x{:x})", MEMPTR(thisptr).GetMPTR(), MEMPTR(serviceToken).GetMPTR());
			cemuLog_logDebug(LogType::Force, "\t->{}", fmt::ptr(serviceToken));
			memcpy(&thisptr->settings[TaskSetting_t::kServiceToken], serviceToken, TaskSetting_t::kServiceTokenLen);
			return 0x200080;
		}

		Result AddInternalCaCert(NetTaskSetting_t* thisptr, char certId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddInternalCaCert(0x{:x}, 0x{:x})", MEMPTR(thisptr).GetMPTR(), (int)certId);

			uint32 location = TaskSetting_t::kCACert;
			for(int i = 0; i < 3; ++i)
			{
				if(thisptr->settings[location] == 0)
				{
					thisptr->settings[location] = (uint8)certId;
					return 0x200080;
				}

				location += TaskSetting_t::kCACert;
			}
		
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_AddInternalCaCert: can't store certificate");
			return 0xA0220D00;
		}

		void SetInternalClientCert(NetTaskSetting_t* thisptr, char certId)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NetTaskSetting_SetInternalClientCert(0x{:x}, 0x{:x})", MEMPTR(thisptr).GetMPTR(), (int)certId);
			thisptr->settings[TaskSetting_t::kClientCert] = (uint8)certId;
		}
	}

	namespace NbdlTaskSetting // : NetTaskSetting
	{
		NbdlTaskSetting_t* ctor(NbdlTaskSetting_t* thisptr)
		{
			if (!thisptr)
			{
				// thisptr = new NbdlTaskSetting_t
				assert_dbg();
			}

			if (thisptr)
			{
				NetTaskSetting::ctor(thisptr);
				thisptr->uknExt_vTableProbably = 0;
			}

			return thisptr;
		}

		void export_ctor(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, NbdlTaskSetting_t, 0);
			cemuLog_logDebug(LogType::Force, "nn_boss_NbdlTaskSetting_ctor");
			ctor(thisptr.GetPtr());
			osLib_returnFromFunction(hCPU, thisptr.GetMPTR());
		}

		void export_Initialize(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, NbdlTaskSetting_t, 0);
			ppcDefineParamMEMPTR(bossCode, const char, 1);
			ppcDefineParamU64(directorySizeLimit, 2);
			ppcDefineParamMEMPTR(directoryName, const char, 4);
			cemuLog_logDebug(LogType::Force, "nn_boss_NbdlTaskSetting_Initialize(0x{:08x}, {}, 0x{:x}, 0x{:08x})", thisptr.GetMPTR(), bossCode.GetPtr(), directorySizeLimit, directoryName.GetMPTR());

			if(!bossCode || strnlen(bossCode.GetPtr(), 0x20) == 0x20)
			{
				osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780));
				return;
			}

			if (directoryName && strnlen(directoryName.GetPtr(), 0x8) == 0x8)
			{
				osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780));
				return;
			}

			strncpy((char*)&thisptr->settings[TaskSetting_t::kBossCode], bossCode.GetPtr(), TaskSetting_t::kBossCodeLen);
			
			*(uint64be*)&thisptr->settings[TaskSetting_t::kDirectorySizeLimit] = directorySizeLimit; // uint64be
			if(directoryName)
				strncpy((char*)&thisptr->settings[TaskSetting_t::kDirectoryName], directoryName.GetPtr(), TaskSetting_t::kDirectoryNameLen);

			osLib_returnFromFunction(hCPU, BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80));
		}

		Result SetFileName(NbdlTaskSetting_t* thisptr, const char* fileName)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_NbdlTaskSetting_t_SetFileName(0x{:08x}, {})", MEMPTR(thisptr).GetMPTR(), fileName ? fileName : "\"\"");

			if (!fileName || strnlen(fileName, TaskSetting_t::kFileNameLen) == TaskSetting_t::kFileNameLen)
			{
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780);
			}
			
			strncpy((char*)&thisptr->settings[TaskSetting_t::kNbdlFileName], fileName, TaskSetting_t::kFileNameLen);
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
		}

	}

	namespace RawUlTaskSetting
	{
		RawUlTaskSetting_t* ctor(RawUlTaskSetting_t* thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_RawUlTaskSetting_ctor(0x{:x}) TODO", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new RawUlTaskSetting_t
				assert_dbg();
			}

			if (thisptr)
			{
				NetTaskSetting::ctor(thisptr);
				thisptr->uknExt_vTableProbably = RawUlTaskSetting_t::kType;
				thisptr->ukRaw1 = 0;
				thisptr->ukRaw2 = 0;
				thisptr->ukRaw3 = 0;
				memset(thisptr->rawSpace, 0x00, 0x200);
			}

			return thisptr;
		}
	}

	namespace RawDlTaskSetting
	{
		RawDlTaskSetting_t* ctor(RawDlTaskSetting_t* thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_RawDlTaskSetting_ctor(0x{:x}) TODO", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new RawDlTaskSetting_t
				assert_dbg();
			}

			if (thisptr)
			{
				NetTaskSetting::ctor(thisptr);
				thisptr->uknExt_vTableProbably = RawDlTaskSetting_t::kType;
			}

			return thisptr;
		}

		Result Initialize(RawDlTaskSetting_t* thisptr, const char* url, bool newArrival, bool led, const char* fileName, const char* directoryName)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_RawDlTaskSetting_Initialize(0x{:x}, 0x{:x}, {}, {}, 0x{:x}, 0x{:x})", MEMPTR(thisptr).GetMPTR(), MEMPTR(url).GetMPTR(), newArrival, led, MEMPTR(fileName).GetMPTR(), MEMPTR(directoryName).GetMPTR());
			if (!url)
			{
				return 0xC0203780;
			}

			if (strnlen(url, TaskSetting_t::kURLLen) == TaskSetting_t::kURLLen)
			{
				return 0xC0203780;
			}

			cemuLog_logDebug(LogType::Force, "\t-> url: {}", url);

			if (fileName && strnlen(fileName, TaskSetting_t::kFileNameLen) == TaskSetting_t::kFileNameLen)
			{
				return 0xC0203780;
			}

			if (directoryName && strnlen(directoryName, TaskSetting_t::kDirectoryNameLen) == TaskSetting_t::kDirectoryNameLen)
			{
				return 0xC0203780;
			}

			strncpy((char*)thisptr + TaskSetting_t::kURL, url, TaskSetting_t::kURLLen);
			thisptr->settings[0x147] = '\0';

			if (fileName)
				strncpy((char*)thisptr + 0x7D0, fileName, TaskSetting_t::kFileNameLen);
			else
				strncpy((char*)thisptr + 0x7D0, "rawcontent.dat", TaskSetting_t::kFileNameLen);
			thisptr->settings[0x7EF] = '\0';

			cemuLog_logDebug(LogType::Force, "\t-> filename: {}", (char*)thisptr + 0x7D0);

			if (directoryName)
			{
				strncpy((char*)thisptr + 0x7C8, directoryName, TaskSetting_t::kDirectoryNameLen);
				thisptr->settings[0x7CF] = '\0';
				cemuLog_logDebug(LogType::Force, "\t-> directoryName: {}", (char*)thisptr + 0x7C8);
			}

			thisptr->settings[0x7C0] = newArrival;
			thisptr->settings[0x7C1] = led;
			*(uint16be*)&thisptr->settings[0x28] = 0x3;
			return 0x200080;
		}
	}

	namespace PlayReportSetting // : NetTaskSetting
	{
		void export_ctor(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, PlayReportSetting_t, 0);
			cemuLog_logDebug(LogType::Force, "nn_boss_PlayReportSetting_ctor TODO");
			if (!thisptr)
			{
				assert_dbg();
			}

			if (thisptr)
			{
				RawUlTaskSetting::ctor(thisptr.GetPtr());
				thisptr->uknExt_vTableProbably = PlayReportSetting_t::kType;
				thisptr->ukPlay1 = nullptr;
				thisptr->ukPlay2 = 0;
				thisptr->ukPlay3 = 0;
				thisptr->ukPlay4 = 0;
			}

			osLib_returnFromFunction(hCPU, thisptr.GetMPTR());
		}

		void export_Initialize(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, PlayReportSetting_t, 0);
			ppcDefineParamMEMPTR(ptr, void*, 1);
			ppcDefineParamU32(value, 2);
			ppcDefineParamMEMPTR(directoryName, const char, 4);
			//cemuLog_logDebug(LogType::Force, "nn_boss_PlayReportSetting_Initialize(0x{:08x}, {}, 0x{:x}, 0x{:08x})", thisptr.GetMPTR(), ptr.GetPtr(), directorySizeLimit, directoryName.GetMPTR());

			if(!ptr || value == 0 || value > 0x19000)
			{
				cemuLog_logDebug(LogType::Force, "nn_boss_PlayReportSetting_Initialize: invalid parameter");
				osLib_returnFromFunction(hCPU, 0);
			}

			*ptr.GetPtr<uint8>() = 0;

			*(uint16be*)&thisptr->settings[0x28] = 6;
			*(uint16be*)&thisptr->settings[0x2B] |= 0x3;
			*(uint16be*)&thisptr->settings[0x2C] |= 0xA;
			*(uint32be*)&thisptr->settings[0x7C0] |= 2;
		
			thisptr->ukPlay1 = ptr;
			thisptr->ukPlay2 = value;
			thisptr->ukPlay3 = 0;
			thisptr->ukPlay4 = 0;

			// TODO
			osLib_returnFromFunction(hCPU, 0);
		}

		void export_Set(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, PlayReportSetting_t, 0);
			ppcDefineParamMEMPTR(key, const char, 1);
			ppcDefineParamU32(value, 2);
			
			// TODO
			cemuLog_logDebug(LogType::Force, "nn_boss_PlayReportSetting_Set(0x{:08x}, {}, 0x{:x}) TODO", thisptr.GetMPTR(), key.GetPtr(), value);
			
			osLib_returnFromFunction(hCPU, 1);
		}

		
	}

	namespace Title
	{
		Title_t* ctor(Title_t* thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_Title_ctor(0x{:x})", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			*thisptr = {};

			return thisptr;
		}
	}

	namespace DirectoryName
	{
		DirectoryName_t* ctor(DirectoryName_t* thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_DirectoryName_ctor(0x{:x})", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			memset(thisptr->name, 0x00, 0x8);

			return thisptr;
		}
		
		const char* operator_const_char(DirectoryName_t* thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_DirectoryName_operator_const_char(0x{:x})", MEMPTR(thisptr).GetMPTR());
			return thisptr->name;
		}
	}

	namespace Task
	{

		Result Initialize(Task_t* thisptr, const char* taskId, uint32 accountId)
		{
			if(!taskId || strnlen(taskId, 0x8) == 8)
			{
				return BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_BOSS, 0x3780);
			}

			thisptr->accountId = accountId;
			strncpy(thisptr->taskId.id, taskId, 0x08);
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_BOSS, 0x80);
		}

		Result Initialize(Task_t* thisptr, uint8 slot, const char* taskId)
		{
			const uint32 accountId = slot == 0 ? 0 : act::GetPersistentIdEx(slot);
			return Initialize(thisptr, taskId, accountId);
		}

		Result Initialize(Task_t* thisptr, const char* taskId)
		{
			return Initialize(thisptr, taskId, 0);
		}

		void export_Initialize3(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(taskId, const char, 1);
			ppcDefineParamU32(accountId, 2);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_Initialize3(0x{:08x}, {}, 0x{:x})", thisptr.GetMPTR(), taskId.GetPtr(), accountId);
			const Result result = Initialize(thisptr.GetPtr(), taskId.GetPtr(), accountId);
			osLib_returnFromFunction(hCPU, result);
		}

		void export_Initialize2(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamU8(slotId, 1);
			ppcDefineParamMEMPTR(taskId, const char, 2);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_Initialize2(0x{:08x}, {}, {})", thisptr.GetMPTR(), slotId, taskId.GetPtr());
			const Result result = Initialize(thisptr.GetPtr(), slotId, taskId.GetPtr());
			osLib_returnFromFunction(hCPU, result);
		}
		
		void export_Initialize1(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(taskId, const char, 1);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_Initialize1(0x{:08x}, {})", thisptr.GetMPTR(), taskId.GetPtr());
			const Result result = Initialize(thisptr.GetPtr(), taskId.GetPtr());
			osLib_returnFromFunction(hCPU, result);
		}

	
		Task_t* ctor(Task_t* thisptr, const char* taskId, uint32 accountId)
		{
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			if (thisptr)
			{
				thisptr->accountId = 0;
				thisptr->ext = 0; // dword_10002174
				TaskId::ctor(&thisptr->taskId);
				TitleId::ctor(&thisptr->titleId, 0);
				cemu_assert_debug(NN_RESULT_IS_SUCCESS(Initialize(thisptr, taskId, accountId)));
			}

			return thisptr;
		}

		Task_t* ctor(Task_t* thisptr, uint8 slot, const char* taskId)
		{
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			if (thisptr)
			{
				thisptr->accountId = 0;
				thisptr->ext = 0; // dword_10002174
				TaskId::ctor(&thisptr->taskId);
				TitleId::ctor(&thisptr->titleId, 0);
				cemu_assert_debug(NN_RESULT_IS_SUCCESS(Initialize(thisptr, slot, taskId)));
			}

			return thisptr;
		}

		Task_t* ctor(Task_t* thisptr, const char* taskId)
		{
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			if (thisptr)
			{
				thisptr->accountId = 0;
				thisptr->ext = 0; // dword_10002174
				TaskId::ctor(&thisptr->taskId);
				TitleId::ctor(&thisptr->titleId, 0);
				cemu_assert_debug(NN_RESULT_IS_SUCCESS(Initialize(thisptr, taskId)));
			}
			
			return thisptr;
		}

		Task_t* ctor(Task_t* thisptr)
		{
			if (!thisptr)
			{
				// thisptr = new Task_t
				assert_dbg();
			}

			if (thisptr)
			{
				thisptr->accountId = 0;
				thisptr->ext = 0; // dword_10002174
				TaskId::ctor(&thisptr->taskId);
				TitleId::ctor(&thisptr->titleId, 0);
				memset(&thisptr->taskId, 0x00, sizeof(TaskId_t));
			}

			return thisptr;
		}

		void export_ctor(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_ctor(0x{:08x})", thisptr.GetMPTR());
			ctor(thisptr.GetPtr());
			osLib_returnFromFunction(hCPU, thisptr.GetMPTR());
		}

		void export_Run(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamU8(isForegroundRun, 1);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_Run(0x{:08x}, {})", thisptr.GetMPTR(), isForegroundRun);
			if (isForegroundRun != 0)
			{
				//peterBreak();
				cemuLog_logDebug(LogType::Force, "export_Run foreground run");
			}

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_RUN;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;
			bossRequest->bool_parameter = isForegroundRun != 0;
			
			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);
	
			osLib_returnFromFunction(hCPU, 0);
		}

		void export_StartScheduling(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamU8(executeImmediately, 1);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_StartScheduling(0x{:08x}, {})", thisptr.GetMPTR(), executeImmediately);

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_START_SCHEDULING;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;
			bossRequest->bool_parameter = executeImmediately != 0;
			
			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);
	
			osLib_returnFromFunction(hCPU, 0);
		}

		void export_StopScheduling(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_StopScheduling(0x{:08x})", thisptr.GetMPTR());

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_STOP_SCHEDULING;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;
			
			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);
	
			osLib_returnFromFunction(hCPU, 0);
		}

		void export_IsRegistered(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_IS_REGISTERED;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->titleId = thisptr->titleId.u64;
			bossRequest->taskId = thisptr->taskId.id;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			cemuLog_logDebug(LogType::Force, "nn_boss_Task_IsRegistered(0x{:08x}) -> {}", thisptr.GetMPTR(), bossRequest->returnCode);

			osLib_returnFromFunction(hCPU, bossRequest->returnCode);
		}

		void export_Wait(PPCInterpreter_t* hCPU)
		{
			// Wait__Q3_2nn4boss4TaskFUiQ3_2nn4boss13TaskWaitState
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamU32(timeout, 1);
			ppcDefineParamU32(waitState, 2);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_Wait(0x{:08x}, 0x{:x}, {})", thisptr.GetMPTR(), timeout, waitState);

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_WAIT;
			bossRequest->titleId = thisptr->titleId.u64;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->timeout = timeout;
			bossRequest->waitState = waitState;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			osLib_returnFromFunction(hCPU, bossRequest->returnCode);

			//osLib_returnFromFunction(hCPU, 1); // 0 -> timeout, 1 -> wait condition met
		}

		void export_RegisterForImmediateRun(PPCInterpreter_t* hCPU)
		{
			// RegisterForImmediateRun__Q3_2nn4boss4TaskFRCQ3_2nn4boss11TaskSetting
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(settings, TaskSetting_t, 1);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_RegisterForImmediateRun(0x{:08x}, 0x{:08x})", thisptr.GetMPTR(), settings.GetMPTR());

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_REGISTER;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->settings = settings.GetPtr();
			bossRequest->uk1 = 0xC00;

			if (TaskSetting::IsPrivilegedTaskSetting(settings.GetPtr()))
				bossRequest->titleId = thisptr->titleId.u64;

			const sint32 result = __depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);
			osLib_returnFromFunction(hCPU, result);
		}

		void export_Unregister(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_Unregister(0x{:08x})", thisptr.GetMPTR());

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_UNREGISTER;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;

			const sint32 result = __depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);
			osLib_returnFromFunction(hCPU, result);
		}

		void export_Register(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(settings, TaskSetting_t, 1);
			cemuLog_logDebug(LogType::Force, "nn_boss_Task_Register(0x{:08x}, 0x{:08x})", thisptr.GetMPTR(), settings.GetMPTR());

			if (hCPU->gpr[4] == 0)
			{
				cemuLog_logDebug(LogType::Force, "nn_boss_Task_Register - crash workaround (fix me)");
				osLib_returnFromFunction(hCPU, 0);
				return;
			}

			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_REGISTER_FOR_IMMEDIATE_RUN;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->settings = settings.GetPtr();
			bossRequest->uk1 = 0xC00;

			if(TaskSetting::IsPrivilegedTaskSetting(settings.GetPtr()))
				bossRequest->titleId = thisptr->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			osLib_returnFromFunction(hCPU, bossRequest->returnCode);
		}
		
		
		void export_GetTurnState(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(execCount, uint32be, 1);
			
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_TURN_STATE;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (execCount)
				*execCount = bossRequest->u32.exec_count;

			cemuLog_logDebug(LogType::Force, "nn_boss_Task_GetTurnState(0x{:08x}, 0x{:08x}) -> {}", thisptr.GetMPTR(), execCount.GetMPTR(), bossRequest->u32.result);

			osLib_returnFromFunction(hCPU, bossRequest->u32.result);
			//osLib_returnFromFunction(hCPU, 7); // 7 -> finished? 0x11 -> Error (Splatoon doesn't like it when we return 0x11 for Nbdl tasks) RETURN FINISHED
		}

		void export_GetContentLength(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(execCount, uint32be, 1);
			
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_CONTENT_LENGTH;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (execCount)
				*execCount = bossRequest->u64.exec_count;

			cemuLog_logDebug(LogType::Force, "nn_boss_Task_GetContentLength(0x{:08x}, 0x{:08x}) -> 0x{:x}", thisptr.GetMPTR(), execCount.GetMPTR(), bossRequest->u64.result);

			osLib_returnFromFunction64(hCPU, bossRequest->u64.result);
		}

		void export_GetProcessedLength(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(execCount, uint32be, 1);
			
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_PROCESSED_LENGTH;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (execCount)
				*execCount = bossRequest->u64.exec_count;

			cemuLog_logDebug(LogType::Force, "nn_boss_Task_GetProcessedLength(0x{:08x}, 0x{:08x}) -> 0x{:x}", thisptr.GetMPTR(), execCount.GetMPTR(), bossRequest->u64.result);

			osLib_returnFromFunction64(hCPU, bossRequest->u64.result);
		}

		void export_GetHttpStatusCode(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(thisptr, Task_t, 0);
			ppcDefineParamMEMPTR(execCount, uint32be, 1);
			
			bossPrepareRequest();
			bossRequest->requestCode = IOSU_NN_BOSS_TASK_GET_HTTP_STATUS_CODE;
			bossRequest->accountId = thisptr->accountId;
			bossRequest->taskId = thisptr->taskId.id;
			bossRequest->titleId = thisptr->titleId.u64;

			__depr__IOS_Ioctlv(IOS_DEVICE_BOSS, IOSU_BOSS_REQUEST_CEMU, 1, 1, bossBufferVector);

			if (execCount)
				*execCount = bossRequest->u32.exec_count;

			cemuLog_logDebug(LogType::Force, "nn_boss_Task_GetHttpStatusCode(0x{:08x}, 0x{:08x}) -> {}", thisptr.GetMPTR(), execCount.GetMPTR(), bossRequest->u32.result);

			osLib_returnFromFunction(hCPU, bossRequest->u32.result);
		}
	}

	struct PrivilegedTask_t : Task_t
	{
		
	};
	static_assert(sizeof(PrivilegedTask_t) == 0x20);
	
	struct AlmightyTask_t : PrivilegedTask_t
	{
		
	};
	static_assert(sizeof(AlmightyTask_t) == 0x20);

	namespace PrivilegedTask
	{
		PrivilegedTask_t* ctor(PrivilegedTask_t*thisptr)
		{
			if (!thisptr)
				assert_dbg(); // new

			Task::ctor(thisptr);
			thisptr->ext = 0x10003a50;
			return thisptr;
		}
	}
	
	namespace AlmightyTask
	{
		AlmightyTask_t* ctor(AlmightyTask_t* thisptr)
		{
			if (!thisptr)
				assert_dbg(); // new

			PrivilegedTask::ctor(thisptr);
			thisptr->ext = 0x10002a0c;
			return thisptr;
		}
		void dtor(AlmightyTask_t* thisptr)
		{
			if (thisptr)
				freeMem(thisptr);
		}
		
		uint32 Initialize(AlmightyTask_t* thisptr, TitleId_t* titleId, const char* taskId, uint32 accountId)
		{
			if (!thisptr)
				return 0xc0203780;

			thisptr->accountId = accountId;
			thisptr->titleId.u64 = titleId->u64;
			strncpy(thisptr->taskId.id, taskId, 8);
			thisptr->taskId.id[7] = 0x00;
			
			return 0x200080;
		}
	}

	Result InitializeImpl()
	{
		// if( Initialize(IpcClientCafe*) ) ...
		g_isInitialized = true;
		return 0;
	}

	void export_IsInitialized(PPCInterpreter_t* hCPU)
	{
		osLib_returnFromFunction(hCPU, (uint32)g_isInitialized);
	}

	Result Initialize()
	{
		Result result;
		coreinit::OSLockMutex(&g_mutex);

		if(g_initCounter != 0 || NN_RESULT_IS_SUCCESS((result=InitializeImpl())))
		{
			g_initCounter++;
			result = 0;
		}

		coreinit::OSUnlockMutex(&g_mutex);
		return result;
	}

	void export_Initialize(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "nn_boss_Initialize()");
		osLib_returnFromFunction(hCPU, Initialize());
	}

	void export_GetBossState(PPCInterpreter_t* hCPU)
	{
		cemuLog_logDebug(LogType::Force, "nn_boss.GetBossState() - stub");
		osLib_returnFromFunction(hCPU, 7);
	}

	enum StorageKind
	{
		kStorageKind_NBDL,
		kStorageKind_RawDl,
	};

	namespace Storage
	{
		struct bossStorage_t
		{
			/* +0x00 */ uint32be accountId;
			/* +0x04 */ uint32be storageKind;
			/* +0x08 */ uint8 ukn08Array[3];
			/* +0x0B */ char storageName[8];
			uint8 ukn13;
			uint8 ukn14;
			uint8 ukn15;
			uint8 ukn16;
			uint8 ukn17;
			/* +0x18 */
			nn::boss::TitleId_t titleId;
			uint32be ukn20; // pointer to some global struct
			uint32be ukn24;
		};
		
		static_assert(sizeof(bossStorage_t) == 0x28);
		static_assert(offsetof(bossStorage_t, storageKind) == 0x04);
		static_assert(offsetof(bossStorage_t, ukn08Array) == 0x08);
		static_assert(offsetof(bossStorage_t, storageName) == 0x0B);
		static_assert(offsetof(bossStorage_t, titleId) == 0x18);

		bossStorage_t* ctor(bossStorage_t* thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_Storage_ctor(0x{:x})", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new RawDlTaskSetting_t
				assert_dbg();
			}

			if (thisptr)
			{
				thisptr->titleId.u64 = 0;
				thisptr->ukn20 = 0x10000a64;
			}

			return thisptr;
		}

		void nnBossStorage_prepareTitleId(bossStorage_t* storage)
		{
			if (storage->titleId.u64 != 0)
				return;
			storage->titleId.u64 = CafeSystem::GetForegroundTitleId();
		}

		Result Initialize(bossStorage_t* thisptr, const char* dirName, uint32 accountId, StorageKind type)
		{
			if (!dirName)
				return 0xC0203780;

			cemuLog_logDebug(LogType::Force, "boss::Storage::Initialize({}, 0x{:08x}, {})", dirName, accountId, type);

			thisptr->storageKind = type;
			thisptr->titleId.u64 = 0;

			memset(thisptr->storageName, 0, 0x8);
			strncpy(thisptr->storageName, dirName, 0x8);
			thisptr->storageName[7] = '\0';

			thisptr->accountId = accountId;

			nnBossStorage_prepareTitleId(thisptr); // usually not done like this

			return 0x200080;
		}

		Result Initialize2(bossStorage_t* thisptr, const char* dirName, StorageKind type)
		{
			return Initialize(thisptr, dirName, 0, type);
		}
	}

	using Storage_t = Storage::bossStorage_t;
	struct AlmightyStorage_t : Storage_t
	{
	};
	static_assert(sizeof(AlmightyStorage_t) == 0x28);

	namespace AlmightyStorage
	{
		AlmightyStorage_t* ctor(AlmightyStorage_t* thisptr)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_AlmightyStorage_ctor(0x{:x})", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
			{
				// thisptr = new RawDlTaskSetting_t
				assert_dbg();
			}

			if (thisptr)
			{
				Storage::ctor(thisptr);
				thisptr->ukn20 = 0x100028a4;
			}

			return thisptr;
		}

		uint32 Initialize(AlmightyStorage_t* thisptr, TitleId_t* titleId, const char* storageName, uint32 accountId, StorageKind storageKind)
		{
			cemuLog_logDebug(LogType::Force, "nn_boss_AlmightyStorage_Initialize(0x{:x})", MEMPTR(thisptr).GetMPTR());
			if (!thisptr)
				return 0xc0203780;

			thisptr->accountId = accountId;
			thisptr->storageKind = storageKind;
			thisptr->titleId.u64 = titleId->u64;
			
			strncpy(thisptr->storageName, storageName, 8);
			thisptr->storageName[0x7] = 0x00;
	
			return 0x200080;
		}
	}
} 
}

// Storage

struct bossDataName_t
{
	char name[32];
};

static_assert(sizeof(bossDataName_t) == 0x20);

struct bossStorageFadEntry_t
{
	char name[32];
	uint32be fileNameId;
	uint32 ukn24;
	uint32 ukn28;
	uint32 ukn2C;
	uint32 ukn30;
	uint32be timestampRelated; // guessed
};

// __ct__Q3_2nn4boss8DataNameFv
void nnBossDataNameExport_ct(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(dataName, bossDataName_t, 0);
	memset(dataName, 0, sizeof(bossDataName_t));
	osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(dataName));
}

// __opPCc__Q3_2nn4boss8DataNameCFv
void nnBossDataNameExport_opPCc(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(dataName, bossDataName_t, 0);
	osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(dataName->name));
}

void nnBossStorageExport_ct(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(storage, nn::boss::Storage::bossStorage_t, 0);
	cemuLog_logDebug(LogType::Force, "Constructor for boss storage called");
	// todo
	memset(storage, 0, sizeof(nn::boss::Storage::bossStorage_t));
	osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(storage));
}

void nnBossStorageExport_exist(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(storage, nn::boss::Storage::bossStorage_t, 0);
	cemuLog_logDebug(LogType::Force, "nn_boss.Storage_Exist(...) TODO");

	// todo
	osLib_returnFromFunction(hCPU, 1);
}

#define FAD_ENTRY_MAX_COUNT		512

FSCVirtualFile* nnBossStorageFile_open(nn::boss::Storage::bossStorage_t* storage, uint32 fileNameId)
{
	char storageFilePath[1024];
	sprintf(storageFilePath, "/cemuBossStorage/%08x/%08x/user/common/data/%s/%08x", (uint32)(storage->titleId.u64 >> 32), (uint32)(storage->titleId.u64), storage->storageName, fileNameId);
	sint32 fscStatus;
	FSCVirtualFile* fscStorageFile = fsc_open(storageFilePath, FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION | FSC_ACCESS_FLAG::WRITE_PERMISSION, &fscStatus);
	return fscStorageFile;
}

bossStorageFadEntry_t* nnBossStorageFad_getTable(nn::boss::Storage::bossStorage_t* storage)
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
	bossStorageFadEntry_t* fadTable = (bossStorageFadEntry_t*)malloc(sizeof(bossStorageFadEntry_t)*FAD_ENTRY_MAX_COUNT);
	memset(fadTable, 0, sizeof(bossStorageFadEntry_t)*FAD_ENTRY_MAX_COUNT);
	fsc_readFile(fscFadFile, fadTable, sizeof(bossStorageFadEntry_t)*FAD_ENTRY_MAX_COUNT);
	fsc_close(fscFadFile);
	return fadTable;
}

// Find index of entry by name. Returns -1 if not found
sint32 nnBossStorageFad_getIndexByName(bossStorageFadEntry_t* fadTable, char* name)
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

bool nnBossStorageFad_getEntryByName(nn::boss::Storage::bossStorage_t* storage, char* name, bossStorageFadEntry_t* fadEntry)
{
	bossStorageFadEntry_t* fadTable = nnBossStorageFad_getTable(storage);
	if (fadTable)
	{
		sint32 entryIndex = nnBossStorageFad_getIndexByName(fadTable, name);
		if (entryIndex >= 0)
		{
			memcpy(fadEntry, fadTable + entryIndex, sizeof(bossStorageFadEntry_t));
			free(fadTable);
			return true;
		}
		free(fadTable);
	}
	return false;
}

void nnBossStorageExport_getDataList(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(storage, nn::boss::Storage::bossStorage_t, 0);
	ppcDefineParamStructPtr(dataList, bossDataName_t, 1);
	ppcDefineParamS32(maxEntries, 2);
	ppcDefineParamU32BEPtr(outputEntryCount, 3);
	cemuLog_logDebug(LogType::Force, "boss storage getDataList()");

	// initialize titleId of storage if not already done
	nnBossStorage_prepareTitleId(storage);

	// load fad.db
	bossStorageFadEntry_t* fadTable = nnBossStorageFad_getTable(storage);
	if (fadTable)
	{
		sint32 validEntryCount = 0;
		for (sint32 i = 0; i < FAD_ENTRY_MAX_COUNT; i++)
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
	osLib_returnFromFunction(hCPU, 0); // error code
}

// NsData

typedef struct  
{
	/* +0x00 */ char name[0x20];
	/* +0x20 */ nn::boss::Storage::bossStorage_t storage;
	/* +0x48 */ uint64 readIndex;
	/* +0x50 */ uint32 ukn50; // some pointer to a global struct
	/* +0x54 */ uint32 ukn54;
}nsData_t;

void nnBossNsDataExport_ct(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nnBossNsDataExport_ct");
	ppcDefineParamStructPtr(nsData, nsData_t, 0);
	if (!nsData)
		assert_dbg();


	nsData->ukn50 = 0x10000530;

	memset(nsData->name, 0, 0x20);
	
	nsData->storage.ukn20 = 0x10000798;
	nsData->storage.titleId.u64 = 0;

	nsData->readIndex = 0;

	osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(nsData));
}

void nnBossNsDataExport_initialize(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(nsData, nsData_t, 0);
	ppcDefineParamStructPtr(storage, nn::boss::Storage::bossStorage_t, 1);
	ppcDefineParamStr(dataName, 2);

	if(dataName == nullptr)
	{
		if (storage->storageKind != 1)
		{
			osLib_returnFromFunction(hCPU, 0xC0203780);
			return;
		}
	}
	
	nsData->storage.accountId = storage->accountId;
	nsData->storage.storageKind = storage->storageKind;

	memcpy(nsData->storage.ukn08Array, storage->ukn08Array, 3);
	memcpy(nsData->storage.storageName, storage->storageName, 8);

	nsData->storage.titleId.u64 = storage->titleId.u64;

	nsData->storage = *storage;

	if (dataName != nullptr || storage->storageKind != 1)
		strncpy(nsData->name, dataName, 0x20);
	else
		strncpy(nsData->name, "rawcontent.dat", 0x20);
	nsData->name[0x1F] = '\0';

	nsData->readIndex = 0;

	cemuLog_logDebug(LogType::Force, "nnBossNsDataExport_initialize: {}", nsData->name);
	
	osLib_returnFromFunction(hCPU, 0x200080);
}

std::string nnBossNsDataExport_GetPath(nsData_t* nsData)
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

void nnBossNsDataExport_DeleteRealFileWithHistory(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(nsData, nsData_t, 0);
	cemuLog_logDebug(LogType::Force, "nn_boss.NsData_DeleteRealFileWithHistory(...)");

	if (nsData->storage.storageKind == nn::boss::kStorageKind_NBDL)
	{
		// todo
		cemuLog_log(LogType::Force, "BOSS NBDL: Unsupported delete");
	}
	else
	{
		sint32 fscStatus = FSC_STATUS_OK;
		std::string filePath = nnBossNsDataExport_GetPath(nsData).c_str();
		fsc_remove((char*)filePath.c_str(), &fscStatus);
		if (fscStatus != 0)
			cemuLog_log(LogType::Force, "Unhandeled FSC status in BOSS DeleteRealFileWithHistory()");
	}
	osLib_returnFromFunction(hCPU, 0);
}

void nnBossNsDataExport_Exist(PPCInterpreter_t* hCPU)
{
	cemuLog_logDebug(LogType::Force, "nn_boss.NsData_Exist(...)");
	ppcDefineParamStructPtr(nsData, nsData_t, 0);

	bool fileExists = false;
	if(nsData->storage.storageKind == nn::boss::kStorageKind_NBDL)
	{
		// check if name is present in fad table
		bossStorageFadEntry_t* fadTable = nnBossStorageFad_getTable(&nsData->storage);
		if (fadTable)
		{
			fileExists = nnBossStorageFad_getIndexByName(fadTable, nsData->name) >= 0;
			cemuLog_logDebug(LogType::Force, "\t({}) -> {}", nsData->name, fileExists);
			free(fadTable);
		}
	}
	else
	{
		sint32 fscStatus;
		auto fscStorageFile = fsc_open((char*)nnBossNsDataExport_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE, &fscStatus);
		if (fscStorageFile != nullptr)
		{
			fileExists = true;
			fsc_close(fscStorageFile);
		}
	}

	osLib_returnFromFunction(hCPU, fileExists?1:0);
}

void nnBossNsDataExport_getSize(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(nsData, nsData_t, 0);

	FSCVirtualFile* fscStorageFile = nullptr;
	if (nsData->storage.storageKind == nn::boss::kStorageKind_NBDL)
	{
		bossStorageFadEntry_t fadEntry;
		if (nnBossStorageFad_getEntryByName(&nsData->storage, nsData->name, &fadEntry) == false)
		{
			cemuLog_log(LogType::Force, "BOSS storage cant find file {}", nsData->name);
			osLib_returnFromFunction(hCPU, 0);
			return;
		}
		// open file
		fscStorageFile = nnBossStorageFile_open(&nsData->storage, fadEntry.fileNameId);
	}
	else
	{
		sint32 fscStatus;
		fscStorageFile = fsc_open((char*)nnBossNsDataExport_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
	}

	if (fscStorageFile == nullptr)
	{
		cemuLog_log(LogType::Force, "BOSS storage cant open file alias {}", nsData->name);
		osLib_returnFromFunction(hCPU, 0);
		return;
	}

	// get size
	const sint32 fileSize = fsc_getFileSize(fscStorageFile);
	// close file
	fsc_close(fscStorageFile);
	osLib_returnFromFunction64(hCPU, fileSize);
}

uint64 nnBossNsData_GetCreatedTime(nsData_t* nsData)
{
	cemuLog_logDebug(LogType::Force, "nn_boss.NsData_GetCreatedTime() not implemented. Returning 0");
	uint64 createdTime = 0;
	return createdTime;
}

uint32 nnBossNsData_read(nsData_t* nsData, uint64* sizeOutBE, void* buffer, sint32 length)
{
	FSCVirtualFile* fscStorageFile = nullptr;
	if (nsData->storage.storageKind == nn::boss::kStorageKind_NBDL)
	{
		bossStorageFadEntry_t fadEntry;
		if (nnBossStorageFad_getEntryByName(&nsData->storage, nsData->name, &fadEntry) == false)
		{
			cemuLog_log(LogType::Force, "BOSS storage cant find file {} for reading", nsData->name);
			return 0x80000000; // todo - proper error code
		}
		// open file
		fscStorageFile = nnBossStorageFile_open(&nsData->storage, fadEntry.fileNameId);
	}
	else
	{
		sint32 fscStatus;
		fscStorageFile = fsc_open((char*)nnBossNsDataExport_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
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
		*sizeOutBE = _swapEndianU64(readBytes);
	return 0;
}

#define NSDATA_SEEK_MODE_BEGINNING	(0)

uint32 nnBossNsData_seek(nsData_t* nsData, uint64 seek, uint32 mode)
{
	FSCVirtualFile* fscStorageFile = nullptr;
	if (nsData->storage.storageKind == nn::boss::kStorageKind_NBDL)
	{
		bossStorageFadEntry_t fadEntry;
		if (nnBossStorageFad_getEntryByName(&nsData->storage, nsData->name, &fadEntry) == false)
		{
			cemuLog_log(LogType::Force, "BOSS storage cant find file {} for reading", nsData->name);
			return 0x80000000; // todo - proper error code
		}
		// open file
		fscStorageFile = nnBossStorageFile_open(&nsData->storage, fadEntry.fileNameId);
	}
	else
	{
		sint32 fscStatus;
		fscStorageFile = fsc_open((char*)nnBossNsDataExport_GetPath(nsData).c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &fscStatus);
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

void nnBossNsDataExport_read(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(nsData, nsData_t, 0);
	ppcDefineParamStr(buffer, 1);
	ppcDefineParamS32(length, 2);

	cemuLog_logDebug(LogType::Force, "nsData read (filename {})", nsData->name);

	uint32 r = nnBossNsData_read(nsData, nullptr, buffer, length);

	osLib_returnFromFunction(hCPU, r);
}

void nnBossNsDataExport_readWithSizeOut(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(nsData, nsData_t, 0);
	ppcDefineParamTypePtr(sizeOut, uint64, 1);
	ppcDefineParamStr(buffer, 2);
	ppcDefineParamS32(length, 3);

	uint32 r = nnBossNsData_read(nsData, sizeOut, buffer, length);
	cemuLog_logDebug(LogType::Force, "nsData readWithSizeOut (filename {} length 0x{:x}) Result: {} Sizeout: {:x}", nsData->name, length, r, _swapEndianU64(*sizeOut));

	osLib_returnFromFunction(hCPU, r);
}

void nnBossNsDataExport_seek(PPCInterpreter_t* hCPU)
{
	ppcDefineParamStructPtr(nsData, nsData_t, 0);
	ppcDefineParamU64(seekPos, 2);
	ppcDefineParamU32(mode, 4);

	uint32 r = nnBossNsData_seek(nsData, seekPos, mode);

	cemuLog_logDebug(LogType::Force, "nsData seek (filename {} seek 0x{:x}) Result: {}", nsData->name, (uint32)seekPos, r);

	osLib_returnFromFunction(hCPU, r);
}

void nnBoss_load()
{
	OSInitMutexEx(&nn::boss::g_mutex, nullptr);

	osLib_addFunction("nn_boss", "Initialize__Q2_2nn4bossFv", nn::boss::export_Initialize);
	osLib_addFunction("nn_boss", "GetBossState__Q2_2nn4bossFv", nn::boss::export_GetBossState);
	
	// task
	osLib_addFunction("nn_boss", "__ct__Q3_2nn4boss4TaskFv", nn::boss::Task::export_ctor);
	osLib_addFunction("nn_boss", "Run__Q3_2nn4boss4TaskFb", nn::boss::Task::export_Run);
	osLib_addFunction("nn_boss", "Wait__Q3_2nn4boss4TaskFUiQ3_2nn4boss13TaskWaitState", nn::boss::Task::export_Wait);
	osLib_addFunction("nn_boss", "GetTurnState__Q3_2nn4boss4TaskCFPUi", nn::boss::Task::export_GetTurnState);
	osLib_addFunction("nn_boss", "GetHttpStatusCode__Q3_2nn4boss4TaskCFPUi", nn::boss::Task::export_GetHttpStatusCode);
	osLib_addFunction("nn_boss", "GetContentLength__Q3_2nn4boss4TaskCFPUi", nn::boss::Task::export_GetContentLength);
	osLib_addFunction("nn_boss", "GetProcessedLength__Q3_2nn4boss4TaskCFPUi", nn::boss::Task::export_GetProcessedLength);
	osLib_addFunction("nn_boss", "Register__Q3_2nn4boss4TaskFRQ3_2nn4boss11TaskSetting", nn::boss::Task::export_Register);
	osLib_addFunction("nn_boss", "Unregister__Q3_2nn4boss4TaskFv", nn::boss::Task::export_Unregister);
	osLib_addFunction("nn_boss", "Initialize__Q3_2nn4boss4TaskFPCc", nn::boss::Task::export_Initialize1);
	osLib_addFunction("nn_boss", "Initialize__Q3_2nn4boss4TaskFUcPCc", nn::boss::Task::export_Initialize2);
	osLib_addFunction("nn_boss", "Initialize__Q3_2nn4boss4TaskFPCcUi", nn::boss::Task::export_Initialize3);
	osLib_addFunction("nn_boss", "IsRegistered__Q3_2nn4boss4TaskCFv", nn::boss::Task::export_IsRegistered);
	osLib_addFunction("nn_boss", "RegisterForImmediateRun__Q3_2nn4boss4TaskFRCQ3_2nn4boss11TaskSetting", nn::boss::Task::export_RegisterForImmediateRun);
	osLib_addFunction("nn_boss", "StartScheduling__Q3_2nn4boss4TaskFb", nn::boss::Task::export_StartScheduling);
	osLib_addFunction("nn_boss", "StopScheduling__Q3_2nn4boss4TaskFv", nn::boss::Task::export_StopScheduling);

	// Nbdl task setting
	osLib_addFunction("nn_boss", "__ct__Q3_2nn4boss15NbdlTaskSettingFv", nn::boss::NbdlTaskSetting::export_ctor);
	osLib_addFunction("nn_boss", "Initialize__Q3_2nn4boss15NbdlTaskSettingFPCcLT1", nn::boss::NbdlTaskSetting::export_Initialize);
	//osLib_addFunction("nn_boss", "SetFileName__Q3_2nn4boss15NbdlTaskSettingFPCc", nn::boss::NbdlTaskSetting::export_SetFileName);
	cafeExportRegisterFunc(nn::boss::NbdlTaskSetting::SetFileName, "nn_boss", "SetFileName__Q3_2nn4boss15NbdlTaskSettingFPCc", LogType::Placeholder);
	
	// play task setting
	osLib_addFunction("nn_boss", "__ct__Q3_2nn4boss17PlayReportSettingFv", nn::boss::PlayReportSetting::export_ctor); 
	osLib_addFunction("nn_boss", "Set__Q3_2nn4boss17PlayReportSettingFPCcUi", nn::boss::PlayReportSetting::export_Set); 
	//osLib_addFunction("nn_boss", "Set__Q3_2nn4boss17PlayReportSettingFUiT1", nn::boss::PlayReportSetting::export_Set); 
	osLib_addFunction("nn_boss", "Initialize__Q3_2nn4boss17PlayReportSettingFPvUi", nn::boss::PlayReportSetting::export_Initialize); 

	cafeExportRegisterFunc(nn::boss::RawDlTaskSetting::ctor, "nn_boss", "__ct__Q3_2nn4boss16RawDlTaskSettingFv", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::RawDlTaskSetting::Initialize, "nn_boss", "Initialize__Q3_2nn4boss16RawDlTaskSettingFPCcbT2N21", LogType::Placeholder);

	cafeExportRegisterFunc(nn::boss::NetTaskSetting::SetServiceToken, "nn_boss", "SetServiceToken__Q3_2nn4boss14NetTaskSettingFPCUc", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::NetTaskSetting::AddInternalCaCert, "nn_boss", "AddInternalCaCert__Q3_2nn4boss14NetTaskSettingFSc", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::NetTaskSetting::SetInternalClientCert, "nn_boss", "SetInternalClientCert__Q3_2nn4boss14NetTaskSettingFSc", LogType::Placeholder);

	// Title
	cafeExportRegisterFunc(nn::boss::Title::ctor, "nn_boss", "__ct__Q3_2nn4boss5TitleFv", LogType::Placeholder);
	// cafeExportMakeWrapper<nn::boss::Title::SetNewArrivalFlagOff>("nn_boss", "SetNewArrivalFlagOff__Q3_2nn4boss5TitleFv"); SMM bookmarks

	// TitleId
	cafeExportRegisterFunc(nn::boss::TitleId::ctor1, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFv", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::TitleId::ctor2, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFUL", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::TitleId::cctor, "nn_boss", "__ct__Q3_2nn4boss7TitleIDFRCQ3_2nn4boss7TitleID", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::TitleId::operator_ne, "nn_boss", "__ne__Q3_2nn4boss7TitleIDCFRCQ3_2nn4boss7TitleID", LogType::Placeholder);

	// DataName
	osLib_addFunction("nn_boss", "__ct__Q3_2nn4boss8DataNameFv", nnBossDataNameExport_ct);
	osLib_addFunction("nn_boss", "__opPCc__Q3_2nn4boss8DataNameCFv", nnBossDataNameExport_opPCc);

	// DirectoryName
	cafeExportRegisterFunc(nn::boss::DirectoryName::ctor, "nn_boss", "__ct__Q3_2nn4boss13DirectoryNameFv", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::DirectoryName::operator_const_char, "nn_boss", "__opPCc__Q3_2nn4boss13DirectoryNameCFv", LogType::Placeholder);

	// Account
	cafeExportRegisterFunc(nn::boss::Account::ctor, "nn_boss", "__ct__Q3_2nn4boss7AccountFUi", LogType::Placeholder);
	
	
	// storage
	osLib_addFunction("nn_boss", "__ct__Q3_2nn4boss7StorageFv", nnBossStorageExport_ct);
	//osLib_addFunction("nn_boss", "Initialize__Q3_2nn4boss7StorageFPCcQ3_2nn4boss11StorageKind", nnBossStorageExport_initialize);
	osLib_addFunction("nn_boss", "Exist__Q3_2nn4boss7StorageCFv", nnBossStorageExport_exist);
	osLib_addFunction("nn_boss", "GetDataList__Q3_2nn4boss7StorageCFPQ3_2nn4boss8DataNameUiPUiT2", nnBossStorageExport_getDataList);
	osLib_addFunction("nn_boss", "GetDataList__Q3_2nn4boss7StorageCFPQ3_2nn4boss8DataNameUiPUiT2", nnBossStorageExport_getDataList);
	cafeExportRegisterFunc(nn::boss::Storage::Initialize, "nn_boss", "Initialize__Q3_2nn4boss7StorageFPCcUiQ3_2nn4boss11StorageKind", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::Storage::Initialize2, "nn_boss", "Initialize__Q3_2nn4boss7StorageFPCcQ3_2nn4boss11StorageKind", LogType::Placeholder);

	// AlmightyStorage
	cafeExportRegisterFunc(nn::boss::AlmightyStorage::ctor, "nn_boss", "__ct__Q3_2nn4boss15AlmightyStorageFv", LogType::Placeholder );
	cafeExportRegisterFunc(nn::boss::AlmightyStorage::Initialize, "nn_boss", "Initialize__Q3_2nn4boss15AlmightyStorageFQ3_2nn4boss7TitleIDPCcUiQ3_2nn4boss11StorageKind", LogType::Placeholder );

	// AlmightyTask
	cafeExportRegisterFunc(nn::boss::AlmightyTask::ctor, "nn_boss", "__ct__Q3_2nn4boss12AlmightyTaskFv", LogType::Placeholder);
	cafeExportRegisterFunc(nn::boss::AlmightyTask::Initialize, "nn_boss", "Initialize__Q3_2nn4boss12AlmightyTaskFQ3_2nn4boss7TitleIDPCcUi", LogType::Placeholder);
	// cafeExportRegisterFunc(nn::boss::AlmightyTask::dtor, "nn_boss", "__dt__Q3_2nn4boss12AlmightyTaskFv", LogType::Placeholder);

	// NsData
	osLib_addFunction("nn_boss", "__ct__Q3_2nn4boss6NsDataFv", nnBossNsDataExport_ct);
	osLib_addFunction("nn_boss", "Initialize__Q3_2nn4boss6NsDataFRCQ3_2nn4boss7StoragePCc", nnBossNsDataExport_initialize);
	osLib_addFunction("nn_boss", "DeleteRealFileWithHistory__Q3_2nn4boss6NsDataFv", nnBossNsDataExport_DeleteRealFileWithHistory);
	osLib_addFunction("nn_boss", "Exist__Q3_2nn4boss6NsDataCFv", nnBossNsDataExport_Exist);
	osLib_addFunction("nn_boss", "GetSize__Q3_2nn4boss6NsDataCFv", nnBossNsDataExport_getSize);
	cafeExportRegisterFunc(nnBossNsData_GetCreatedTime, "nn_boss", "GetCreatedTime__Q3_2nn4boss6NsDataCFv", LogType::Placeholder);
	osLib_addFunction("nn_boss", "Read__Q3_2nn4boss6NsDataFPvUi", nnBossNsDataExport_read);
	osLib_addFunction("nn_boss", "Read__Q3_2nn4boss6NsDataFPLPvUi", nnBossNsDataExport_readWithSizeOut);
	osLib_addFunction("nn_boss", "Seek__Q3_2nn4boss6NsDataFLQ3_2nn4boss12PositionBase", nnBossNsDataExport_seek);

}
