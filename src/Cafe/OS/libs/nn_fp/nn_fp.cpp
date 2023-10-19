#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/IOSU/legacy/iosu_act.h"
#include "Cafe/IOSU/legacy/iosu_fpd.h"
#include "Cafe/IOSU/legacy/iosu_ioctl.h" // deprecated
#include "Cafe/IOSU/iosu_ipc_common.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"
#include "Cafe/OS/libs/coreinit/coreinit_IPC.h"
#include "Cafe/OS/libs/nn_common.h"
#include "util/ChunkedHeap/ChunkedHeap.h"
#include "Common/CafeString.h"

namespace nn
{
	namespace fp
	{
		static const auto FPResult_OkZero = 0;
		static const auto FPResult_Ok = BUILD_NN_RESULT(NN_RESULT_LEVEL_SUCCESS, NN_RESULT_MODULE_NN_FP, 0);
		static const auto FPResult_InvalidIPCParam = BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_FP, 0x680);
		static const auto FPResult_RequestFailed = BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_FP, 0); // figure out proper error code

		struct
		{
			uint32 initCounter;
			bool isAdminMode;
			bool isLoggedIn;
			IOSDevHandle fpdHandle;
			SysAllocator<coreinit::OSMutex> fpMutex;
			SysAllocator<uint8, 0x12000> g_fpdAllocatorSpace;
			VHeap* fpBufferHeap{nullptr};
			// PPC buffers for async notification query
			SysAllocator<uint32be> notificationCount;
			SysAllocator<iosu::fpd::FPDNotification, 256> notificationBuffer;
			bool getNotificationCalled{false};
			// notification handler
			MEMPTR<void> notificationHandler{nullptr};
			MEMPTR<void> notificationHandlerParam{nullptr};
		}g_fp = { };

		class
		{
		  public:
			void Init()
			{
				std::unique_lock _l(m_mtx);
				g_fp.fpBufferHeap = new VHeap(g_fp.g_fpdAllocatorSpace.GetPtr(), g_fp.g_fpdAllocatorSpace.GetByteSize());
			}

			void Destroy()
			{
				std::unique_lock _l(m_mtx);
				delete g_fp.fpBufferHeap;
			}

			void* Allocate(uint32 size, uint32 alignment)
			{
				std::unique_lock _l(m_mtx);
				void* p = g_fp.fpBufferHeap->alloc(size, 32);
				if (!p)
					cemuLog_log(LogType::Force, "nn_fp: Internal heap is full");
				return p;
			}

			void Free(void* ptr)
			{
				std::unique_lock _l(m_mtx);
				g_fp.fpBufferHeap->free(ptr);
			}

		  private:
			std::mutex m_mtx;
		}FPIpcBufferAllocator;

		class FPIpcContext {
			static inline constexpr uint32 MAX_VEC_COUNT = 8;
		  public:
			// use FP heap for this class
			static void* operator new(size_t size)
			{
				return FPIpcBufferAllocator.Allocate(size, (uint32)alignof(FPIpcContext));
			}

			static void operator delete(void* ptr)
			{
				FPIpcBufferAllocator.Free(ptr);
			}

			FPIpcContext(iosu::fpd::FPD_REQUEST_ID requestId) : m_requestId(requestId)
			{
			}

			~FPIpcContext()
			{
				if(m_dataBuffer)
					FPIpcBufferAllocator.Free(m_dataBuffer);
			}

			void AddInput(void* ptr, uint32 size)
			{
				size_t vecIndex = GetVecInIndex(m_numVecIn);
				m_vec[vecIndex].baseVirt = ptr;
				m_vec[vecIndex].size = size;
				m_numVecIn = m_numVecIn + 1;
			}

			void AddOutput(void* ptr, uint32 size)
			{
				cemu_assert_debug(m_numVecIn == 0); // all outputs need to be added before any inputs
				size_t vecIndex = GetVecOutIndex(m_numVecOut);
				m_vec[vecIndex].baseVirt = ptr;
				m_vec[vecIndex].size = size;
				m_numVecOut = m_numVecOut + 1;
			}

			uint32 Submit(std::unique_ptr<FPIpcContext> owner)
			{
				InitSubmissionBuffer();
				// note: While generally, Ioctlv() usage has the order as input (app->IOSU) followed by output (IOSU->app), FP uses it the other way around
				nnResult r = coreinit::IOS_Ioctlv(g_fp.fpdHandle, (uint32)m_requestId.value(), m_numVecOut, m_numVecIn, m_vec);
				CopyBackOutputs();
				owner.reset();
				return r;
			}

			nnResult SubmitAsync(std::unique_ptr<FPIpcContext> owner, MEMPTR<void> callbackFunc, MEMPTR<void> callbackParam)
			{
				InitSubmissionBuffer();
				this->m_callbackFunc = callbackFunc;
				this->m_callbackParam = callbackParam;
				nnResult r = coreinit::IOS_IoctlvAsync(g_fp.fpdHandle, (uint32)m_requestId.value(), m_numVecOut, m_numVecIn, m_vec, MEMPTR<void>(PPCInterpreter_makeCallableExportDepr(AsyncHandler)), MEMPTR<void>(this));
				owner.release();
				return r;
			}

		  private:
			size_t GetVecInIndex(uint8 inIndex)
			{
				return m_numVecOut + inIndex;
			}

			size_t GetVecOutIndex(uint8 outIndex)
			{
				return outIndex;
			}

			void InitSubmissionBuffer()
			{
				// allocate a chunk of memory to hold the input/output vectors and their data
				uint32 vecOffset[MAX_VEC_COUNT];
				uint32 totalBufferSize = 0;
				for(uint8 i=0; i<m_numVecIn + m_numVecOut; i++)
				{
					vecOffset[i] = totalBufferSize;
					totalBufferSize += m_vec[i].size;
					totalBufferSize = (totalBufferSize+31)&~31;
				}
				if(totalBufferSize > 0)
				{
					m_dataBuffer = FPIpcBufferAllocator.Allocate(totalBufferSize, 32);
					cemu_assert_debug(m_dataBuffer);
				}
				// update Ioctl vector addresses
				for(uint8 i=0; i<m_numVecIn + m_numVecOut; i++)
				{
					void* bufferAddr = (uint8be*)m_dataBuffer.GetPtr() + vecOffset[i];
					m_vecOriginalAddress[i] = m_vec[i].baseVirt;
					m_vec[i].baseVirt = bufferAddr;
				}
				// copy input data to buffer
				for(uint8 i=0; i<m_numVecIn; i++)
				{
					uint8 vecIndex = GetVecInIndex(i);
					memcpy(MEMPTR<void>(m_vec[vecIndex].baseVirt).GetPtr(), MEMPTR<void>(m_vecOriginalAddress[vecIndex]).GetPtr(), m_vec[vecIndex].size);
				}
			}

			static void AsyncHandler(PPCInterpreter_t* hCPU)
			{
				ppcDefineParamU32(result, 0);
				ppcDefineParamPtr(ipcCtx, FPIpcContext, 1);
				ipcCtx->m_asyncResult = result; // store result in variable since FP callbacks pass a pointer to nnResult and not the value directly
				ipcCtx->CopyBackOutputs();
				PPCCoreCallback(ipcCtx->m_callbackFunc, &ipcCtx->m_asyncResult, ipcCtx->m_callbackParam);
				delete ipcCtx;
				osLib_returnFromFunction(hCPU, 0);
			}

			void CopyBackOutputs()
			{
				if(m_numVecOut > 0)
				{
					// copy output from temporary output buffers to the original addresses
					for(uint8 i=0; i<m_numVecOut; i++)
					{
						uint32 vecOffset = (uint32)m_vec[GetVecOutIndex(i)].baseVirt.GetMPTR() - (uint32)m_vec[0].baseVirt.GetMPTR();
						memcpy(m_vecOriginalAddress[GetVecOutIndex(i)].GetPtr(), (uint8be*)m_dataBuffer.GetPtr() + vecOffset, m_vec[GetVecOutIndex(i)].size);
					}
				}
			}

			betype<iosu::fpd::FPD_REQUEST_ID> m_requestId;
			uint8be m_numVecIn{0};
			uint8be m_numVecOut{0};
			IPCIoctlVector m_vec[MAX_VEC_COUNT];
			MEMPTR<void> m_vecOriginalAddress[MAX_VEC_COUNT]{};
			MEMPTR<void> m_dataBuffer{nullptr};
			MEMPTR<void> m_callbackFunc{nullptr};
			MEMPTR<void> m_callbackParam{nullptr};
			betype<nnResult> m_asyncResult;
		};

		struct FPGlobalLock
		{
			FPGlobalLock()
			{
				coreinit::OSLockMutex(&g_fp.fpMutex);
			}
			~FPGlobalLock()
			{
				coreinit::OSUnlockMutex(&g_fp.fpMutex);
			}
		};
		#define FP_API_BASE() if (g_fp.initCounter == 0) return 0xC0C00580; FPGlobalLock _fpLock;
		#define FP_API_BASE_ZeroOnError() if (g_fp.initCounter == 0) return 0; FPGlobalLock _fpLock;

		nnResult Initialize()
		{
			FPGlobalLock _fpLock;
			if (g_fp.initCounter == 0)
			{
				g_fp.fpdHandle = coreinit::IOS_Open("/dev/fpd", 0);
			}
			g_fp.initCounter++;
			return FPResult_OkZero;
		}

		uint32 IsInitialized()
		{
			FPGlobalLock _fpLock;
			return g_fp.initCounter > 0 ? 1 : 0;
		}

		nnResult InitializeAdmin(PPCInterpreter_t* hCPU)
		{
			FPGlobalLock _fpLock;
			g_fp.isAdminMode = true;
			return Initialize();
		}

		uint32 IsInitializedAdmin()
		{
			FPGlobalLock _fpLock;
			return g_fp.initCounter > 0 ? 1 : 0;
		}

		nnResult Finalize()
		{
			FPGlobalLock _fpLock;
			if (g_fp.initCounter == 1)
			{
				g_fp.initCounter = 0;
				g_fp.isAdminMode = false;
				g_fp.isLoggedIn = false;
				coreinit::IOS_Close(g_fp.fpdHandle);
				g_fp.getNotificationCalled = false;
			}
			else if (g_fp.initCounter > 0)
				g_fp.initCounter--;
			return FPResult_OkZero;
		}

		nnResult FinalizeAdmin()
		{
			return Finalize();
		}

		void GetNextNotificationAsync();

		nnResult SetNotificationHandler(uint32 notificationMask, void* funcPtr, void* userParam)
		{
			FP_API_BASE();
			g_fp.notificationHandler = funcPtr;
			g_fp.notificationHandlerParam = userParam;
			StackAllocator<uint32be> notificationMaskBuf; notificationMaskBuf = notificationMask;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::SetNotificationMask);
			ipcCtx->AddInput(&notificationMaskBuf, sizeof(uint32be));
			nnResult r = ipcCtx->Submit(std::move(ipcCtx));
			if (NN_RESULT_IS_SUCCESS(r))
			{
				// async query for notifications
				GetNextNotificationAsync();
			}
			return r;
		}

		void GetNextNotificationAsyncHandler(PPCInterpreter_t* hCPU)
		{
			coreinit::OSLockMutex(&g_fp.fpMutex);
			cemu_assert_debug(g_fp.getNotificationCalled);
			g_fp.getNotificationCalled = false;
			auto bufPtr = g_fp.notificationBuffer.GetPtr();
			uint32 count = g_fp.notificationCount->value();
			if (count == 0)
			{
				GetNextNotificationAsync();
				coreinit::OSUnlockMutex(&g_fp.fpMutex);
				osLib_returnFromFunction(hCPU, 0);
				return;
			}
			// copy notifications to temporary buffer using std::copy
			iosu::fpd::FPDNotification tempBuffer[256];
			std::copy(g_fp.notificationBuffer.GetPtr(), g_fp.notificationBuffer.GetPtr() + count, tempBuffer);
			// call handler for each notification, but do it outside of the lock
			void* notificationHandler = g_fp.notificationHandler;
			void* notificationHandlerParam = g_fp.notificationHandlerParam;
			coreinit::OSUnlockMutex(&g_fp.fpMutex);
			iosu::fpd::FPDNotification* notificationBuffer = g_fp.notificationBuffer.GetPtr();
			for (uint32 i = 0; i < count; i++)
				PPCCoreCallback(notificationHandler, (uint32)notificationBuffer[i].type, notificationBuffer[i].pid, notificationHandlerParam);
			coreinit::OSLockMutex(&g_fp.fpMutex);
			// query more notifications
			GetNextNotificationAsync();
			coreinit::OSUnlockMutex(&g_fp.fpMutex);
			osLib_returnFromFunction(hCPU, 0);
		}

		void GetNextNotificationAsync()
		{
			if (g_fp.getNotificationCalled)
				return;
			g_fp.getNotificationCalled = true;
			g_fp.notificationCount = 0;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetNotificationAsync);
			ipcCtx->AddOutput(g_fp.notificationBuffer.GetPtr(), g_fp.notificationBuffer.GetByteSize());
			ipcCtx->AddOutput(g_fp.notificationCount.GetPtr(), sizeof(uint32be));
			cemu_assert_debug(g_fp.notificationBuffer.GetByteSize() == 0x800);
			nnResult r = ipcCtx->SubmitAsync(std::move(ipcCtx), MEMPTR<void>(PPCInterpreter_makeCallableExportDepr(GetNextNotificationAsyncHandler)), nullptr);
		}

		nnResult LoginAsync(void* funcPtr, void* userParam)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::LoginAsync);
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, userParam);
		}

		uint32 HasLoggedIn()
		{
			FP_API_BASE_ZeroOnError();
			// Sonic All Star Racing uses this
			// and Monster Hunter 3 Ultimate needs this to return false at least once to initiate login and not get stuck
			// this returns false until LoginAsync was called and has completed (?) even if the user is already logged in
			StackAllocator<uint32be> resultBuf;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::HasLoggedIn);
			ipcCtx->AddOutput(&resultBuf, sizeof(uint32be));
			ipcCtx->Submit(std::move(ipcCtx));
			return resultBuf != 0 ? 1 : 0;
		}

		uint32 IsOnline()
		{
			FP_API_BASE_ZeroOnError();
			StackAllocator<uint32be> resultBuf;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::IsOnline);
			ipcCtx->AddOutput(&resultBuf, sizeof(uint32be));
			ipcCtx->Submit(std::move(ipcCtx));
			return resultBuf != 0 ? 1 : 0;
		}

		nnResult GetFriendList(uint32be* pidList, uint32be* returnedCount, uint32 startIndex, uint32 maxCount)
		{
			FP_API_BASE();
			StackAllocator<uint32be> startIndexBuf; startIndexBuf = startIndex;
			StackAllocator<uint32be> maxCountBuf; maxCountBuf = maxCount;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendList);
			ipcCtx->AddOutput(pidList, sizeof(uint32be) * maxCount);
			ipcCtx->AddOutput(returnedCount, sizeof(uint32be));
			ipcCtx->AddInput(&startIndexBuf, sizeof(uint32be));
			ipcCtx->AddInput(&maxCountBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendRequestList(uint32be* pidList, uint32be* returnedCount, uint32 startIndex, uint32 maxCount)
		{
			FP_API_BASE();
			StackAllocator<uint32be> startIndexBuf; startIndexBuf = startIndex;
			StackAllocator<uint32be> maxCountBuf; maxCountBuf = maxCount;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendRequestList);
			ipcCtx->AddOutput(pidList, sizeof(uint32be) * maxCount);
			ipcCtx->AddOutput(returnedCount, sizeof(uint32be));
			ipcCtx->AddInput(&startIndexBuf, sizeof(uint32be));
			ipcCtx->AddInput(&maxCountBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendListAll(uint32be* pidList, uint32be* returnedCount, uint32 startIndex, uint32 maxCount)
		{
			FP_API_BASE();
			StackAllocator<uint32be> startIndexBuf; startIndexBuf = startIndex;
			StackAllocator<uint32be> maxCountBuf; maxCountBuf = maxCount;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendListAll);
			ipcCtx->AddOutput(pidList, sizeof(uint32be) * maxCount);
			ipcCtx->AddOutput(returnedCount, sizeof(uint32be));
			ipcCtx->AddInput(&startIndexBuf, sizeof(uint32be));
			ipcCtx->AddInput(&maxCountBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendListEx(iosu::fpd::FriendData* friendData, uint32be* pidList, uint32 count)
		{
			FP_API_BASE();
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendListEx);
			ipcCtx->AddOutput(friendData, sizeof(iosu::fpd::FriendData) * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendRequestListEx(iosu::fpd::FriendRequest* friendRequest, uint32be* pidList, uint32 count)
		{
			FP_API_BASE();
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendRequestListEx);
			ipcCtx->AddOutput(friendRequest, sizeof(iosu::fpd::FriendRequest) * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetBasicInfoAsync(iosu::fpd::FriendBasicInfo* basicInfo, uint32be* pidList, uint32 count, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetBasicInfoAsync);
			ipcCtx->AddOutput(basicInfo, sizeof(iosu::fpd::FriendBasicInfo) * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		uint32 GetMyPrincipalId()
		{
			FP_API_BASE_ZeroOnError();
			StackAllocator<uint32be> resultBuf;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetMyPrincipalId);
			ipcCtx->AddOutput(&resultBuf, sizeof(uint32be));
			ipcCtx->Submit(std::move(ipcCtx));
			return resultBuf->value();
		}

		nnResult GetMyAccountId(uint8be* accountId)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetMyAccountId);
			ipcCtx->AddOutput(accountId, ACT_ACCOUNTID_LENGTH);
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetMyScreenName(uint16be* screenname)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetMyScreenName);
			ipcCtx->AddOutput(screenname, ACT_NICKNAME_SIZE*sizeof(uint16));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetMyPreference(iosu::fpd::FPDPreference* myPreference)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetMyPreference);
			ipcCtx->AddOutput(myPreference, sizeof(iosu::fpd::FPDPreference));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetMyMii(FFLData_t* fflData)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetMyMii);
			ipcCtx->AddOutput(fflData, sizeof(FFLData_t));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendAccountId(uint8be* accountIdArray, uint32be* pidList, uint32 count)
		{
			FP_API_BASE();
			if (count == 0)
				return 0;
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendAccountId);
			ipcCtx->AddOutput(accountIdArray, ACT_ACCOUNTID_LENGTH * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendScreenName(uint16be* nameList, uint32be* pidList, uint32 count, uint8 replaceNonAscii, uint8be* languageList)
		{
			FP_API_BASE();
			if (count == 0)
				return 0;
			StackAllocator<uint32be> countBuf; countBuf = count;
			StackAllocator<uint32be> replaceNonAsciiBuf; replaceNonAsciiBuf = replaceNonAscii;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendScreenName);
			ipcCtx->AddOutput(nameList, ACT_NICKNAME_SIZE * sizeof(uint16be) * count);
			ipcCtx->AddOutput(languageList, languageList ? sizeof(uint8be) * count : 0);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			ipcCtx->AddInput(&replaceNonAsciiBuf, sizeof(uint8be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendMii(FFLData_t* miiList, uint32be* pidList, uint32 count)
		{
			FP_API_BASE();
			if(count == 0)
				return 0;
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendMii);
			ipcCtx->AddOutput(miiList, sizeof(FFLData_t) * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendPresence(iosu::fpd::FriendPresence* presenceList, uint32be* pidList, uint32 count)
		{
			FP_API_BASE();
			if(count == 0)
				return 0;
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendPresence);
			ipcCtx->AddOutput(presenceList, sizeof(iosu::fpd::FriendPresence) * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult GetFriendRelationship(uint8* relationshipList, uint32be* pidList, uint32 count)
		{
			FP_API_BASE();
			if(count == 0)
				return 0;
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetFriendRelationship);
			ipcCtx->AddOutput(relationshipList, sizeof(uint8) * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		uint32 IsJoinable(iosu::fpd::FriendPresence* presence, uint64 joinMask)
		{
			if (presence->isValid == 0 ||
				presence->isOnline == 0 ||
				presence->gameMode.joinGameId == 0 ||
				presence->gameMode.matchmakeType == 0 ||
				presence->gameMode.groupId == 0 ||
				presence->gameMode.joinGameMode >= 64 )
			{
				return 0;
			}

			uint32 joinGameMode = presence->gameMode.joinGameMode;
			uint64 joinModeMask = (1ULL<<joinGameMode);
			if ((joinModeMask&joinMask) == 0)
				return 0;

			// check relation ship
			uint32 joinFlagMask = presence->gameMode.joinFlagMask;
			if (joinFlagMask == 0)
				return 0;
			if (joinFlagMask == 1)
				return 1;
			if (joinFlagMask == 2)
			{
				// check relationship
				uint8 relationship[1] = { 0 };
				StackAllocator<uint32be, 1> pidList;
				pidList = presence->gameMode.hostPid;
				GetFriendRelationship(relationship, &pidList, 1);
				if(relationship[0] == iosu::fpd::RELATIONSHIP_FRIEND)
					return 1;
				return 0;
			}
			if (joinFlagMask == 0x65 || joinFlagMask == 0x66)
			{
				cemuLog_log(LogType::Force, "Unsupported friend invite");
			}
			return 0;
		}

		nnResult CheckSettingStatusAsync(uint8* status, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::CheckSettingStatusAsync);
			ipcCtx->AddOutput(status, sizeof(uint8be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		uint32 IsPreferenceValid()
		{
			FP_API_BASE_ZeroOnError();
			StackAllocator<uint32be> resultBuf;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::IsPreferenceValid);
			ipcCtx->AddOutput(&resultBuf, sizeof(uint32be));
			ipcCtx->Submit(std::move(ipcCtx));
			return resultBuf != 0 ? 1 : 0;
		}

		nnResult UpdatePreferenceAsync(iosu::fpd::FPDPreference* newPreference, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::UpdatePreferenceAsync);
			ipcCtx->AddInput(newPreference, sizeof(iosu::fpd::FPDPreference));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		nnResult UpdateGameModeWithUnusedParam(iosu::fpd::GameMode* gameMode, uint16be* gameModeMessage, uint32 unusedParam)
		{
			FP_API_BASE();
			uint32 messageLen = CafeStringHelpers::Length(gameModeMessage, iosu::fpd::GAMEMODE_MAX_MESSAGE_LENGTH);
			if(messageLen >= iosu::fpd::GAMEMODE_MAX_MESSAGE_LENGTH)
			{
				cemuLog_log(LogType::Force, "UpdateGameMode: message too long");
				return FPResult_InvalidIPCParam;
			}
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::UpdateGameModeVariation2);
			ipcCtx->AddInput(gameMode, sizeof(iosu::fpd::GameMode));
			ipcCtx->AddInput(gameModeMessage, sizeof(uint16be) * (messageLen + 1));
			return ipcCtx->Submit(std::move(ipcCtx));
		}

		nnResult UpdateGameMode(iosu::fpd::GameMode* gameMode, uint16be* gameModeMessage)
		{
			return UpdateGameModeWithUnusedParam(gameMode, gameModeMessage, 0);
		}

		nnResult GetRequestBlockSettingAsync(uint8* blockSettingList, uint32be* pidList, uint32 count, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::GetRequestBlockSettingAsync);
			ipcCtx->AddOutput(blockSettingList, sizeof(uint8be) * count);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		// overload of AddFriendAsync
		nnResult AddFriendAsyncByPid(uint32 pid, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint32be> pidBuf; pidBuf = pid;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::AddFriendAsyncByPid);
			ipcCtx->AddInput(&pidBuf, sizeof(uint32be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		nnResult DeleteFriendFlagsAsync(uint32be* pidList, uint32 pidCount, uint32 ukn, void* funcPtr, void* customParam)
		{
			// admin function?
			FP_API_BASE();
			StackAllocator<uint32be> pidCountBuf; pidCountBuf = pidCount;
			StackAllocator<uint32be> uknBuf; uknBuf = ukn;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::DeleteFriendFlagsAsync);
			ipcCtx->AddInput(pidList, sizeof(uint32be) * pidCount);
			ipcCtx->AddInput(&pidCountBuf, sizeof(uint32be));
			ipcCtx->AddInput(&uknBuf, sizeof(uint32be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		// overload of AddFriendRequestAsync
		nnResult AddFriendRequestByPlayRecordAsync(iosu::fpd::RecentPlayRecordEx* playRecord, uint16be* message, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::AddFriendRequestByPlayRecordAsync);
			uint32 messageLen = 0;
			while(message[messageLen] != 0)
				messageLen++;
			ipcCtx->AddInput(playRecord, sizeof(iosu::fpd::RecentPlayRecordEx));
			ipcCtx->AddInput(message, sizeof(uint16be) * (messageLen+1));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		nnResult RemoveFriendAsync(uint32 pid, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint32be> pidBuf; pidBuf = pid;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::RemoveFriendAsync);
			ipcCtx->AddInput(&pidBuf, sizeof(uint32be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		nnResult MarkFriendRequestsAsReceivedAsync(uint64be* messageIdList, uint32 count, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint32be> countBuf; countBuf = count;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::MarkFriendRequestsAsReceivedAsync);
			ipcCtx->AddInput(messageIdList, sizeof(uint64be) * count);
			ipcCtx->AddInput(&countBuf, sizeof(uint32be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		nnResult CancelFriendRequestAsync(uint64 requestId, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint64be> requestIdBuf; requestIdBuf = requestId;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::CancelFriendRequestAsync);
			ipcCtx->AddInput(&requestIdBuf, sizeof(uint64be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		nnResult DeleteFriendRequestAsync(uint64 requestId, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint64be> requestIdBuf; requestIdBuf = requestId;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::DeleteFriendRequestAsync);
			ipcCtx->AddInput(&requestIdBuf, sizeof(uint64be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		nnResult AcceptFriendRequestAsync(uint64 requestId, void* funcPtr, void* customParam)
		{
			FP_API_BASE();
			StackAllocator<uint64be> requestIdBuf; requestIdBuf = requestId;
			auto ipcCtx = std::make_unique<FPIpcContext>(iosu::fpd::FPD_REQUEST_ID::AcceptFriendRequestAsync);
			ipcCtx->AddInput(&requestIdBuf, sizeof(uint64be));
			return ipcCtx->SubmitAsync(std::move(ipcCtx), funcPtr, customParam);
		}

		void load()
		{
			g_fp.initCounter = 0;
			g_fp.isAdminMode = false;
			g_fp.isLoggedIn = false;
			g_fp.getNotificationCalled = false;
			g_fp.notificationHandler = nullptr;
			g_fp.notificationHandlerParam = nullptr;

			coreinit::OSInitMutex(&g_fp.fpMutex);
			FPIpcBufferAllocator.Init();

			cafeExportRegisterFunc(Initialize, "nn_fp", "Initialize__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(InitializeAdmin, "nn_fp", "InitializeAdmin__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(IsInitialized, "nn_fp", "IsInitialized__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(IsInitializedAdmin, "nn_fp", "IsInitializedAdmin__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(Finalize, "nn_fp", "Finalize__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(FinalizeAdmin, "nn_fp", "FinalizeAdmin__Q2_2nn2fpFv", LogType::NN_FP);

			cafeExportRegisterFunc(SetNotificationHandler, "nn_fp", "SetNotificationHandler__Q2_2nn2fpFUiPFQ3_2nn2fp16NotificationTypeUiPv_vPv", LogType::NN_FP);

			cafeExportRegisterFunc(LoginAsync, "nn_fp", "LoginAsync__Q2_2nn2fpFPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(HasLoggedIn, "nn_fp", "HasLoggedIn__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(IsOnline, "nn_fp", "IsOnline__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendList, "nn_fp", "GetFriendList__Q2_2nn2fpFPUiT1UiT3", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendRequestList, "nn_fp", "GetFriendRequestList__Q2_2nn2fpFPUiT1UiT3", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendListAll, "nn_fp", "GetFriendListAll__Q2_2nn2fpFPUiT1UiT3", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendListEx, "nn_fp", "GetFriendListEx__Q2_2nn2fpFPQ3_2nn2fp10FriendDataPCUiUi", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendRequestListEx, "nn_fp", "GetFriendRequestListEx__Q2_2nn2fpFPQ3_2nn2fp13FriendRequestPCUiUi", LogType::NN_FP);
			cafeExportRegisterFunc(GetBasicInfoAsync, "nn_fp", "GetBasicInfoAsync__Q2_2nn2fpFPQ3_2nn2fp9BasicInfoPCUiUiPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);

			cafeExportRegisterFunc(GetMyPrincipalId, "nn_fp", "GetMyPrincipalId__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(GetMyAccountId, "nn_fp", "GetMyAccountId__Q2_2nn2fpFPc", LogType::NN_FP);
			cafeExportRegisterFunc(GetMyScreenName, "nn_fp", "GetMyScreenName__Q2_2nn2fpFPw", LogType::NN_FP);
			cafeExportRegisterFunc(GetMyMii, "nn_fp", "GetMyMii__Q2_2nn2fpFP12FFLStoreData", LogType::NN_FP);
			cafeExportRegisterFunc(GetMyPreference, "nn_fp", "GetMyPreference__Q2_2nn2fpFPQ3_2nn2fp10Preference", LogType::NN_FP);

			cafeExportRegisterFunc(GetFriendAccountId, "nn_fp", "GetFriendAccountId__Q2_2nn2fpFPA17_cPCUiUi", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendScreenName, "nn_fp", "GetFriendScreenName__Q2_2nn2fpFPA11_wPCUiUibPUc", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendMii, "nn_fp", "GetFriendMii__Q2_2nn2fpFP12FFLStoreDataPCUiUi", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendPresence, "nn_fp", "GetFriendPresence__Q2_2nn2fpFPQ3_2nn2fp14FriendPresencePCUiUi", LogType::NN_FP);
			cafeExportRegisterFunc(GetFriendRelationship, "nn_fp", "GetFriendRelationship__Q2_2nn2fpFPUcPCUiUi", LogType::NN_FP);
			cafeExportRegisterFunc(IsJoinable, "nn_fp", "IsJoinable__Q2_2nn2fpFPCQ3_2nn2fp14FriendPresenceUL", LogType::NN_FP);

			cafeExportRegisterFunc(CheckSettingStatusAsync, "nn_fp", "CheckSettingStatusAsync__Q2_2nn2fpFPUcPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(IsPreferenceValid, "nn_fp", "IsPreferenceValid__Q2_2nn2fpFv", LogType::NN_FP);
			cafeExportRegisterFunc(UpdatePreferenceAsync, "nn_fp", "UpdatePreferenceAsync__Q2_2nn2fpFPCQ3_2nn2fp10PreferencePFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(GetRequestBlockSettingAsync, "nn_fp", "GetRequestBlockSettingAsync__Q2_2nn2fpFPUcPCUiUiPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);

			cafeExportRegisterFunc(UpdateGameModeWithUnusedParam, "nn_fp", "UpdateGameMode__Q2_2nn2fpFPCQ3_2nn2fp8GameModePCwUi", LogType::NN_FP);
			cafeExportRegisterFunc(UpdateGameMode, "nn_fp", "UpdateGameMode__Q2_2nn2fpFPCQ3_2nn2fp8GameModePCw", LogType::NN_FP);

			cafeExportRegisterFunc(AddFriendAsyncByPid, "nn_fp", "AddFriendAsync__Q2_2nn2fpFUiPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(AddFriendRequestByPlayRecordAsync, "nn_fp", "AddFriendRequestAsync__Q2_2nn2fpFPCQ3_2nn2fp18RecentPlayRecordExPCwPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(DeleteFriendFlagsAsync, "nn_fp", "DeleteFriendFlagsAsync__Q2_2nn2fpFPCUiUiT2PFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(RemoveFriendAsync, "nn_fp", "RemoveFriendAsync__Q2_2nn2fpFUiPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(MarkFriendRequestsAsReceivedAsync, "nn_fp", "MarkFriendRequestsAsReceivedAsync__Q2_2nn2fpFPCULUiPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(CancelFriendRequestAsync, "nn_fp", "CancelFriendRequestAsync__Q2_2nn2fpFULPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(DeleteFriendRequestAsync, "nn_fp", "DeleteFriendRequestAsync__Q2_2nn2fpFULPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
			cafeExportRegisterFunc(AcceptFriendRequestAsync, "nn_fp", "AcceptFriendRequestAsync__Q2_2nn2fpFULPFQ2_2nn6ResultPv_vPv", LogType::NN_FP);
		}
	}
}

