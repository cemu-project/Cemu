#include "Common/precompiled.h"
#include "Cafe/OS/common/OSCommon.h"
#include "camera.h"

#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "util/helpers/helpers.h"

#include "util/helpers/ringbuffer.h"
#include "camera/CameraManager.h"
#include "Cafe/HW/Espresso/Const.h"
#include "Common/CafeString.h"

namespace camera
{
	constexpr unsigned CAMERA_WIDTH = 640;
	constexpr unsigned CAMERA_HEIGHT = 480;

	enum CAMStatus : sint32
	{
		CAM_STATUS_SUCCESS = 0,
		CAM_STATUS_INVALID_ARG = -1,
		CAM_STATUS_INVALID_HANDLE = -2,
		CAM_STATUS_SURFACE_QUEUE_FULL = -4,
		CAM_STATUS_INSUFFICIENT_MEMORY = -5,
		CAM_STATUS_NOT_READY = -6,
		CAM_STATUS_UNINITIALIZED = -8,
		CAM_STATUS_UVC_ERROR = -9,
		CAM_STATUS_DECODER_INIT_INIT_FAILED = -10,
		CAM_STATUS_DEVICE_IN_USE = -12,
		CAM_STATUS_DECODER_SESSION_FAILED = -13,
		CAM_STATUS_INVALID_PROPERTY = -14,
		CAM_STATUS_SEGMENT_VIOLATION = -15
	};

	enum class CAMFps : uint32
	{
		_15 = 0,
		_30 = 1
	};

	enum class CAMEventType : uint32
	{
		Decode = 0,
		Detached = 1
	};

	enum class CAMForceDisplay
	{
		None = 0,
		DRC = 1
	};

	enum class CAMImageType : uint32
	{
		Default = 0
	};

	struct CAMImageInfo
	{
		betype<CAMImageType> type;
		uint32be height;
		uint32be width;
	};
	static_assert(sizeof(CAMImageInfo) == 0x0C);

	struct CAMInitInfo_t
	{
		CAMImageInfo imageInfo;
		uint32be workMemorySize;
		MEMPTR<void> workMemoryData;
		MEMPTR<void> callback;
		betype<CAMForceDisplay> forceDisplay;
		betype<CAMFps> fps;
		uint32be threadFlags;
		uint8 unk[0x10];
	};
	static_assert(sizeof(CAMInitInfo_t) == 0x34);

	struct CAMTargetSurface
	{
		sint32be size;
		MEMPTR<uint8> data;
		uint8 unused[0x18];
	};
	static_assert(sizeof(CAMTargetSurface) == 0x20);

	struct CAMDecodeEventParam
	{
		betype<CAMEventType> type;
		MEMPTR<void> data;
		uint32be channel;
		uint32be errored;
	};
	static_assert(sizeof(CAMDecodeEventParam) == 0x10);

	constexpr static int32_t CAM_HANDLE = 0;

	struct
	{
		std::recursive_mutex mutex{};
		bool initialized = false;
		bool shouldTriggerCallback = false;
		std::atomic_bool isOpen = false;
		std::atomic_bool isExiting = false;
		bool isWorking = false;
		unsigned fps = 30;
		MEMPTR<void> eventCallback = nullptr;
		RingBuffer<MEMPTR<uint8>, 20> inTargetBuffers{};
		RingBuffer<MEMPTR<uint8>, 20> outTargetBuffers{};
	} s_instance;

	SysAllocator<CAMDecodeEventParam> s_cameraEventData;
	SysAllocator<OSThread_t> s_cameraWorkerThread;
	SysAllocator<uint8, 1024 * 64> s_cameraWorkerThreadStack;
	SysAllocator<CafeString<22>> s_cameraWorkerThreadNameBuffer;
	SysAllocator<coreinit::OSEvent> s_cameraOpenEvent;

	void WorkerThread(PPCInterpreter_t*)
	{
		s_cameraEventData->type = CAMEventType::Decode;
		s_cameraEventData->channel = 0;
		s_cameraEventData->data = nullptr;
		s_cameraEventData->errored = false;
		PPCCoreCallback(s_instance.eventCallback, s_cameraEventData.GetMPTR());

		while (!s_instance.isExiting)
		{
			coreinit::OSWaitEvent(s_cameraOpenEvent);
			while (true)
			{
				if (!s_instance.isOpen || s_instance.isExiting)
				{
					// Fill leftover buffers before stopping
					if (!s_instance.inTargetBuffers.HasData())
						break;
				}
				s_cameraEventData->type = CAMEventType::Decode;
				s_cameraEventData->channel = 0;

				const auto surfaceBuffer = s_instance.inTargetBuffers.Pop();
				if (surfaceBuffer.IsNull())
				{
					s_cameraEventData->data = nullptr;
					s_cameraEventData->errored = true;
				}
				else
				{
					CameraManager::FillNV12Buffer(surfaceBuffer.GetPtr());
					s_cameraEventData->data = surfaceBuffer;
					s_cameraEventData->errored = false;
				}
				PPCCoreCallback(s_instance.eventCallback, s_cameraEventData.GetMPTR());
				coreinit::OSSleepTicks(Espresso::TIMER_CLOCK / (s_instance.fps - 1));
			}
		}
		coreinit::OSExitThread(0);
	}

	sint32 CAMGetMemReq(const CAMImageInfo* info)
	{
		if (!info)
			return CAM_STATUS_INVALID_ARG;
		return 1 * 1024; // always return 1KB
	}

	CAMStatus CAMCheckMemSegmentation(void* base, uint32 size)
	{
		if (!base || size == 0)
			return CAM_STATUS_INVALID_ARG;
		return CAM_STATUS_SUCCESS;
	}

	sint32 CAMInit(uint32 cameraId, const CAMInitInfo_t* initInfo, betype<CAMStatus>* error)
	{
		*error = CAM_STATUS_SUCCESS;
		std::scoped_lock lock(s_instance.mutex);
		if (s_instance.initialized)
		{
			*error = CAM_STATUS_DEVICE_IN_USE;
			return -1;
		}

		if (!initInfo || !initInfo->workMemoryData ||
			!match_any_of(initInfo->forceDisplay, CAMForceDisplay::None, CAMForceDisplay::DRC) ||
			!match_any_of(initInfo->fps, CAMFps::_15, CAMFps::_30) ||
			initInfo->imageInfo.type != CAMImageType::Default)
		{
			*error = CAM_STATUS_INVALID_ARG;
			return -1;
		}
		CameraManager::Init();

		cemu_assert_debug(initInfo->forceDisplay != CAMForceDisplay::DRC);
		cemu_assert_debug(initInfo->workMemorySize != 0);
		cemu_assert_debug(initInfo->imageInfo.type == CAMImageType::Default);

		s_instance.isExiting = false;
		s_instance.fps = initInfo->fps == CAMFps::_15 ? 15 : 30;
		s_instance.initialized = true;
		s_instance.eventCallback = initInfo->callback;

		coreinit::OSInitEvent(s_cameraOpenEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_AUTO);

		coreinit::__OSCreateThreadType(
			s_cameraWorkerThread, RPLLoader_MakePPCCallable(WorkerThread), 0, nullptr,
			s_cameraWorkerThreadStack.GetPtr() + s_cameraWorkerThreadStack.GetByteSize(), s_cameraWorkerThreadStack.GetByteSize(),
			0x10, initInfo->threadFlags & 7, OSThread_t::THREAD_TYPE::TYPE_DRIVER);
		s_cameraWorkerThreadNameBuffer->assign("CameraWorkerThread");
		coreinit::OSSetThreadName(s_cameraWorkerThread.GetPtr(), s_cameraWorkerThreadNameBuffer->c_str());
		coreinit::OSResumeThread(s_cameraWorkerThread.GetPtr());
		return CAM_STATUS_SUCCESS;
	}

	CAMStatus CAMClose(sint32 camHandle)
	{
		if (camHandle != CAM_HANDLE)
			return CAM_STATUS_INVALID_HANDLE;
		{
			std::scoped_lock lock(s_instance.mutex);
			if (!s_instance.initialized || !s_instance.isOpen)
				return CAM_STATUS_UNINITIALIZED;
			s_instance.isOpen = false;
		}
		CameraManager::Close();
		return CAM_STATUS_SUCCESS;
	}

	CAMStatus CAMOpen(sint32 camHandle)
	{
		if (camHandle != CAM_HANDLE)
			return CAM_STATUS_INVALID_HANDLE;
		auto lock = std::scoped_lock(s_instance.mutex);
		if (!s_instance.initialized)
			return CAM_STATUS_UNINITIALIZED;
		if (s_instance.isOpen)
			return CAM_STATUS_DEVICE_IN_USE;
		CameraManager::Open();
		s_instance.isOpen = true;
		coreinit::OSSignalEvent(s_cameraOpenEvent);
		s_instance.inTargetBuffers.Clear();
		s_instance.outTargetBuffers.Clear();
		return CAM_STATUS_SUCCESS;
	}

	CAMStatus CAMSubmitTargetSurface(sint32 camHandle, CAMTargetSurface* targetSurface)
	{
		if (camHandle != CAM_HANDLE)
			return CAM_STATUS_INVALID_HANDLE;
		if (!targetSurface || targetSurface->data.IsNull() || targetSurface->size < 1)
			return CAM_STATUS_INVALID_ARG;
		auto lock = std::scoped_lock(s_instance.mutex);
		if (!s_instance.initialized)
			return CAM_STATUS_UNINITIALIZED;
		if (!s_instance.inTargetBuffers.Push(targetSurface->data))
			return CAM_STATUS_SURFACE_QUEUE_FULL;
		return CAM_STATUS_SUCCESS;
	}

	void CAMExit(sint32 camHandle)
	{
		if (camHandle != CAM_HANDLE)
			return;
		std::scoped_lock lock(s_instance.mutex);
		if (!s_instance.initialized)
			return;
		s_instance.isExiting = true;
		if (s_instance.isOpen)
			CAMClose(camHandle);
		coreinit::OSSignalEvent(s_cameraOpenEvent.GetPtr());
		coreinit::OSJoinThread(s_cameraWorkerThread, nullptr);
		s_instance.initialized = false;
	}

	void reset()
	{
		CAMExit(0);
	}

	void load()
	{
		reset();
		cafeExportRegister("camera", CAMGetMemReq, LogType::CameraAPI);
		cafeExportRegister("camera", CAMCheckMemSegmentation, LogType::CameraAPI);
		cafeExportRegister("camera", CAMInit, LogType::CameraAPI);
		cafeExportRegister("camera", CAMExit, LogType::CameraAPI);
		cafeExportRegister("camera", CAMOpen, LogType::CameraAPI);
		cafeExportRegister("camera", CAMClose, LogType::CameraAPI);
		cafeExportRegister("camera", CAMSubmitTargetSurface, LogType::CameraAPI);
	}
} // namespace camera
