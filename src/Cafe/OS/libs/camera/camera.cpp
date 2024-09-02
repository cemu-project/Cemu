#include "Common/precompiled.h"
#include "Cafe/OS/common/OSCommon.h"
#include "camera.h"

#include "Rgb2Nv12.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "util/helpers/helpers.h"

#include <util/helpers/ringbuffer.h>
#include <openpnp-capture.h>

namespace camera
{
	constexpr static size_t g_width = 640;
	constexpr static size_t g_height = 480;
	constexpr static size_t g_pitch = 768;

	enum class CAMError : sint32
	{
		Success = 0,
		InvalidArg = -1,
		InvalidHandle = -2,
		SurfaceQueueFull = -4,
		InsufficientMemory = -5,
		NotReady = -6,
		Uninitialized = -8,
		DeviceInitFailed = -9,
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
		MEMPTR<void> workMemory;
		MEMPTR<void> callback;
		betype<CAMForceDisplay> forceDisplay;
		betype<CAMFps> fps;
		uint32be threadFlags;
		uint8 unk[0x10];
	};
	static_assert(sizeof(CAMInitInfo_t) == 0x34);

	struct CAMTargetSurface
	{
		/* +0x00 */ sint32be surfaceSize;
		/* +0x04 */ MEMPTR<void> surfacePtr;
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
		MEMPTR<void> buffer;
		uint32be channel;
		uint32be errored;
	};
	static_assert(sizeof(CAMDecodeEventParam) == 0x10);

	std::recursive_mutex g_cameraMutex;

	SysAllocator<CAMDecodeEventParam> g_cameraEventData;
	SysAllocator<coreinit::OSAlarm_t> g_cameraAlarm;

	void DecodeAlarmCallback(PPCInterpreter_t*);

	class CAMInstance
	{
		constexpr static auto surfaceBufferSize = g_pitch * g_height * 3 / 2;

	  public:
		CAMInstance(CAMFps frameRate, MEMPTR<void> callbackPtr)
			: m_capCtx(Cap_createContext()),
			  m_capNv12Buffer(surfaceBufferSize),
			  m_frameRate(30),
			  m_callbackPtr(callbackPtr),
			  m_alarm(OSAllocFromSystem(sizeof(coreinit::OSAlarm_t), 64))
		{
			if (callbackPtr.IsNull() || frameRate != CAMFps::_15 && frameRate != CAMFps::_30)
				throw CAMError::InvalidArg;
			coreinit::OSCreateAlarm(g_cameraAlarm.GetPtr());
			coreinit::OSSetPeriodicAlarm(g_cameraAlarm.GetPtr(),
										 coreinit::OSGetTime(),
										 coreinit::EspressoTime::GetTimerClock() / m_frameRate,
										 RPLLoader_MakePPCCallable(DecodeAlarmCallback));
		}
		~CAMInstance()
		{
			m_capWorker.request_stop();
			m_capWorker.join();
			coreinit::OSCancelAlarm(m_alarm);
			OSFreeToSystem(m_alarm.GetMPTR());
			Cap_releaseContext(m_capCtx);
		}

		void OnAlarm()
		{
			std::scoped_lock captureLock(m_capMutex);
			const auto surface = m_targetSurfaceQueue.Pop();
			if (surface.IsNull())
				return;
			std::memcpy(surface->surfacePtr.GetPtr(), m_capNv12Buffer.data(), m_capNv12Buffer.size());
			g_cameraEventData->type = CAMEventType::Decode;
			g_cameraEventData->buffer = surface->surfacePtr;
			g_cameraEventData->channel = 0;
			g_cameraEventData->errored = false;
			PPCCoreCallback(m_callbackPtr, g_cameraEventData.GetPtr());
		}

		void Worker(std::stop_token stopToken, CapFormatID formatId)
		{
			SetThreadName("CAMWorker");
			auto stream = Cap_openStream(m_capCtx, m_capDeviceId, formatId);
			std::vector<uint8> m_capBuffer(g_width * g_height * 3);
			while (!stopToken.stop_requested())
			{
				if (!m_opened)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
					std::this_thread::yield();
					continue;
				}
				std::scoped_lock lock(m_capMutex);
				Cap_captureFrame(m_capCtx, stream, m_capBuffer.data(), m_capBuffer.size());
				Rgb2Nv12(m_capBuffer.data(), g_width, g_height, m_capNv12Buffer.data(), g_pitch);
			}
			Cap_closeStream(m_capCtx, stream);
		}

		CAMError Open()
		{
			std::scoped_lock lock(m_capMutex);
			if (m_opened)
				return CAMError::DeviceInUse;
			const auto formatCount = Cap_getNumFormats(m_capCtx, m_capDeviceId);
			int formatId = -1;
			for (auto i = 0; i < formatCount; ++i)
			{
				CapFormatInfo formatInfo;
				Cap_getFormatInfo(m_capCtx, m_capDeviceId, i, &formatInfo);
				if (formatInfo.width == g_width && formatInfo.height == g_height && formatInfo.fps == m_frameRate)
				{
					formatId = i;
					break;
				}
			}
			if (formatId == -1)
			{
				cemuLog_log(LogType::Force, "camera: Open failed, {}x{} @ {} fps not supported by host camera", g_width, g_height, m_frameRate);
				return CAMError::DeviceInitFailed;
			}
			m_targetSurfaceQueue.Clear();
			m_opened = true;
			m_capWorker = std::jthread(&CAMInstance::Worker, this, formatId);
			return CAMError::Success;
		}

		CAMError Close()
		{
			std::scoped_lock lock(m_capMutex);
			m_opened = false;
			return CAMError::Success;
		}

		CAMError SubmitTargetSurface(MEMPTR<CAMTargetSurface> surface)
		{
			cemu_assert(surface->surfaceSize >= surfaceBufferSize);
			if (!m_opened)
				return CAMError::NotReady;
			if (!m_targetSurfaceQueue.Push(surface))
				return CAMError::SurfaceQueueFull;
			std::cout << "Surface submitted" << std::endl;
			return CAMError::Success;
		}

	  private:
		CapContext m_capCtx;
		CapDeviceID m_capDeviceId = 0;
		std::vector<uint8> m_capNv12Buffer;
		std::mutex m_capMutex{};
		std::jthread m_capWorker;

		uint32 m_frameRate;
		MEMPTR<void> m_callbackPtr;
		RingBuffer<MEMPTR<CAMTargetSurface>, 20> m_targetSurfaceQueue{};
		MEMPTR<coreinit::OSAlarm_t> m_alarm;
		bool m_opened = false;
	};
	std::optional<CAMInstance> g_camInstance;

	void DecodeAlarmCallback(PPCInterpreter_t*)
	{
		std::scoped_lock camLock(g_cameraMutex);
		if (!g_camInstance)
			return;
		g_camInstance->OnAlarm();
	}

	sint32 CAMGetMemReq(CAMImageInfo*)
	{
		return 1 * 1024; // always return 1KB
	}

	CAMError CAMCheckMemSegmentation(void* base, uint32 size)
	{
		return CAMError::Success; // always return success
	}

	sint32 CAMInit(uint32 cameraId, CAMInitInfo_t* camInitInfo, betype<CAMError>* error)
	{
		if (g_camInstance)
		{
			*error = CAMError::DeviceInUse;
			return -1;
		}
		std::unique_lock _lock(g_cameraMutex);
		const auto& [t, height, width] = camInitInfo->imageInfo;
		cemu_assert(width == g_width && height == g_height);
		try
		{
			g_camInstance.emplace(camInitInfo->fps, camInitInfo->callback);
		}
		catch (CAMError e)
		{
			*error = e;
			return -1;
		}
		*error = CAMError::Success;
		return 0;
	}

	void CAMExit(sint32 camHandle)
	{
		if (camHandle != 0 || !g_camInstance)
			return;
		g_camInstance.reset();
	}

	CAMError CAMClose(sint32 camHandle)
	{
		if (camHandle != 0)
			return CAMError::InvalidHandle;
		std::scoped_lock lock(g_cameraMutex);
		if (!g_camInstance)
			return CAMError::Uninitialized;
		return g_camInstance->Close();
	}

	CAMError CAMOpen(sint32 camHandle)
	{
		if (camHandle != 0)
			return CAMError::InvalidHandle;
		auto lock = std::scoped_lock(g_cameraMutex);
		if (!g_camInstance)
			return CAMError::Uninitialized;
		return g_camInstance->Open();
	}

	CAMError CAMSubmitTargetSurface(sint32 camHandle, CAMTargetSurface* targetSurface)
	{
		if (camHandle != 0)
			return CAMError::InvalidHandle;
		if (!targetSurface || targetSurface->surfacePtr.IsNull() || targetSurface->surfaceSize < 1)
			return CAMError::InvalidArg;
		auto lock = std::scoped_lock(g_cameraMutex);
		if (!g_camInstance)
			return CAMError::Uninitialized;
		return g_camInstance->SubmitTargetSurface(targetSurface);
	}

	void reset()
	{
		g_camInstance.reset();
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
