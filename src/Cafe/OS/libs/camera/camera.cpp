#include "Common/precompiled.h"
#include "Cafe/OS/common/OSCommon.h"
#include "camera.h"

#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "util/helpers/helpers.h"

#include "util/helpers/ringbuffer.h"
#include "camera/CameraManager.h"
#include "Cafe/HW/Espresso/Const.h"

namespace camera
{
	constexpr unsigned CAMERA_WIDTH = 640;
	constexpr unsigned CAMERA_HEIGHT = 480;

	enum class CAMStatus : sint32
	{
		Success = 0,
		InvalidArg = -1,
		InvalidHandle = -2,
		SurfaceQueueFull = -4,
		InsufficientMemory = -5,
		NotReady = -6,
		Uninitialized = -8,
		UVCError = -9,
		DecoderInitFailed = -10,
		DeviceInUse = -12,
		DecoderSessionFailed = -13,
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
		/* +0x00 */ sint32be size;
		/* +0x04 */ MEMPTR<uint8_t> data;
		/* +0x08 */ uint32be height;
		/* +0x0C */ uint32be width;
		/* +0x10 */ uint32be ukn10;
		/* +0x14 */ uint32be ukn14;
		/* +0x18 */ uint32be ukn18;
		/* +0x1C */ uint32be ukn1C;
	};

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
		RingBuffer<MEMPTR<uint8_t>, 20> inTargetBuffers{};
		RingBuffer<MEMPTR<uint8_t>, 20> outTargetBuffers{};
	} s_instance;

	SysAllocator<CAMDecodeEventParam> s_cameraEventData;
	SysAllocator<OSThread_t> s_cameraWorkerThread;
	SysAllocator<uint8, 1024 * 64> s_cameraWorkerThreadStack;
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

				auto surfaceBuffer = s_instance.inTargetBuffers.Pop();
				if (surfaceBuffer.IsNull())
				{
					s_cameraEventData->data = nullptr;
					s_cameraEventData->errored = true;
				}
				else
				{
					CameraManager::instance().GetNV12Data(surfaceBuffer.GetPtr());
					s_cameraEventData->data = surfaceBuffer;
					s_cameraEventData->errored = false;
				}
				PPCCoreCallback(s_instance.eventCallback, s_cameraEventData.GetMPTR());
				coreinit::OSSleepTicks(Espresso::TIMER_CLOCK / (s_instance.fps - 1));
			}
		}
		cemuLog_logDebug(LogType::Force, "Camera Worker Thread Exited");
	}

	sint32 CAMGetMemReq(CAMImageInfo*)
	{
		return 1 * 1024; // always return 1KB
	}

	CAMStatus CAMCheckMemSegmentation(void* base, uint32 size)
	{
		if (!base || size == 0)
			return CAMStatus::InvalidArg;
		return CAMStatus::Success;
	}

	sint32 CAMInit(uint32 cameraId, CAMInitInfo_t* initInfo, betype<CAMStatus>* error)
	{
		*error = CAMStatus::Success;
		std::scoped_lock lock(s_instance.mutex);
		if (s_instance.initialized)
		{
			*error = CAMStatus::DeviceInUse;
			return -1;
		}

		if (!initInfo || !initInfo->workMemoryData ||
			!match_any_of(initInfo->forceDisplay, CAMForceDisplay::None, CAMForceDisplay::DRC) ||
			!match_any_of(initInfo->fps, CAMFps::_15, CAMFps::_30) ||
			initInfo->imageInfo.type != CAMImageType::Default)
		{
			*error = CAMStatus::InvalidArg;
			return -1;
		}

		cemu_assert_debug(initInfo->forceDisplay != CAMForceDisplay::DRC);
		cemu_assert_debug(initInfo->workMemorySize != 0);
		cemu_assert_debug(initInfo->imageInfo.type == CAMImageType::Default);

		s_instance.fps = initInfo->fps == CAMFps::_15 ? 15 : 30;
		s_instance.initialized = true;
		s_instance.eventCallback = initInfo->callback;
		coreinit::OSInitEvent(s_cameraOpenEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_AUTO);
		coreinit::__OSCreateThreadType(
			s_cameraWorkerThread, RPLLoader_MakePPCCallable(WorkerThread), 0, nullptr,
			s_cameraWorkerThreadStack.GetPtr() + s_cameraWorkerThreadStack.GetByteSize(), s_cameraWorkerThreadStack.GetByteSize(),
			0x10, initInfo->threadFlags & 7, OSThread_t::THREAD_TYPE::TYPE_DRIVER);
		coreinit::OSResumeThread(s_cameraWorkerThread);
		return 0;
	}

	CAMStatus CAMClose(sint32 camHandle)
	{
		if (camHandle != CAM_HANDLE)
			return CAMStatus::InvalidHandle;
		{
			std::scoped_lock lock(s_instance.mutex);
			if (!s_instance.initialized || !s_instance.isOpen)
				return CAMStatus::Uninitialized;
			s_instance.isOpen = false;
		}
		CameraManager::instance().Close();
		return CAMStatus::Success;
	}

	CAMStatus CAMOpen(sint32 camHandle)
	{
		if (camHandle != CAM_HANDLE)
			return CAMStatus::InvalidHandle;
		auto lock = std::scoped_lock(s_instance.mutex);
		if (!s_instance.initialized)
			return CAMStatus::Uninitialized;
		if (s_instance.isOpen)
			return CAMStatus::DeviceInUse;
		if (!CameraManager::instance().Open(false))
			return CAMStatus::UVCError;
		s_instance.isOpen = true;
		coreinit::OSSignalEvent(s_cameraOpenEvent);
		s_instance.inTargetBuffers.Clear();
		s_instance.outTargetBuffers.Clear();
		return CAMStatus::Success;
	}

	CAMStatus CAMSubmitTargetSurface(sint32 camHandle, CAMTargetSurface* targetSurface)
	{
		if (camHandle != CAM_HANDLE)
			return CAMStatus::InvalidHandle;
		if (!targetSurface || targetSurface->data.IsNull() || targetSurface->size < 1)
			return CAMStatus::InvalidArg;
		auto lock = std::scoped_lock(s_instance.mutex);
		if (!s_instance.initialized)
			return CAMStatus::Uninitialized;
		if (!s_instance.inTargetBuffers.Push(targetSurface->data))
			return CAMStatus::SurfaceQueueFull;
		return CAMStatus::Success;
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
		s_instance.initialized = false;
	}

	void reset()
	{
		CAMExit(0);
	}

	void load()
	{
		reset();
		cafeExportRegister("camera", CAMGetMemReq, LogType::Force);
		cafeExportRegister("camera", CAMCheckMemSegmentation, LogType::Force);
		cafeExportRegister("camera", CAMInit, LogType::Force);
		cafeExportRegister("camera", CAMExit, LogType::Force);
		cafeExportRegister("camera", CAMOpen, LogType::Force);
		cafeExportRegister("camera", CAMClose, LogType::Force);
		cafeExportRegister("camera", CAMSubmitTargetSurface, LogType::Force);
	}
} // namespace camera
