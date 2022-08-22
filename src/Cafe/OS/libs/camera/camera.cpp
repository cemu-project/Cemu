#include "Common/precompiled.h"
#include "Cafe/OS/common/OSCommon.h"
#include "camera.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/HW/Espresso/PPCCallback.h"

namespace camera
{

	struct CAMInitInfo_t
	{
		/* +0x00 */ uint32be ukn00;
		/* +0x04 */ uint32be width;
		/* +0x08 */ uint32be height;
		
		/* +0x0C */ uint32be workMemorySize;
		/* +0x10 */ MEMPTR<void> workMemory;

		/* +0x14 */ uint32be handlerFuncPtr;

		/* +0x18 */ uint32be ukn18;
		/* +0x1C */ uint32be fps;

		/* +0x20 */ uint32be ukn20;
	};

	struct CAMTargetSurface 
	{
		/* +0x00 */ uint32be surfaceSize;
		/* +0x04 */ MEMPTR<void> surfacePtr;
		/* +0x08 */ uint32be ukn08;
		/* +0x0C */ uint32be ukn0C;
		/* +0x10 */ uint32be ukn10;
		/* +0x14 */ uint32be ukn14;
		/* +0x18 */ uint32be ukn18;
		/* +0x1C */ uint32be ukn1C;
	};

	struct CAMCallbackParam 
	{
		// type 0 - frame decoded | field1 - imagePtr, field2 - imageSize, field3 - ukn (0)
		// type 1 - ???


		/* +0x0 */ uint32be type; // 0 -> Frame decoded
		/* +0x4 */ uint32be field1;
		/* +0x8 */ uint32be field2;
		/* +0xC */ uint32be field3;
	};


	#define CAM_ERROR_SUCCESS			0
	#define CAM_ERROR_INVALID_HANDLE		-8

	std::vector<struct CameraInstance*> g_table_cameraHandles;
	std::vector<struct CameraInstance*> g_activeCameraInstances;
	std::recursive_mutex g_mutex_camera;
	std::atomic_int g_cameraCounter{ 0 };
	SysAllocator<coreinit::OSAlarm_t, 1> g_alarm_camera;
	SysAllocator<CAMCallbackParam, 1> g_cameraHandlerParam;

	CameraInstance* GetCameraInstanceByHandle(sint32 camHandle)
	{
		std::unique_lock<std::recursive_mutex> _lock(g_mutex_camera);
		if (camHandle <= 0)
			return nullptr;
		camHandle -= 1;
		if (camHandle >= g_table_cameraHandles.size())
			return nullptr;
		return g_table_cameraHandles[camHandle];
	}

	struct CameraInstance 
	{
		CameraInstance(uint32 frameWidth, uint32 frameHeight, MPTR handlerFunc) : width(frameWidth), height(frameHeight), handlerFunc(handlerFunc) { AcquireHandle(); };
		~CameraInstance() { if (isOpen) { CloseCam(); } ReleaseHandle(); };

		sint32 handle{ 0 };
		uint32 width;
		uint32 height;
		bool isOpen{false};
		std::queue<CAMTargetSurface> queue_targetSurfaces;
		MPTR handlerFunc;

		bool OpenCam()
		{
			if (isOpen)
				return false;
			isOpen = true;
			g_activeCameraInstances.push_back(this);
			return true;
		}

		bool CloseCam()
		{
			if (!isOpen)
				return false;
			isOpen = false;
			vectorRemoveByValue(g_activeCameraInstances, this);
			return true;
		}

		void QueueTargetSurface(CAMTargetSurface* targetSurface)
		{
			std::unique_lock<std::recursive_mutex> _lock(g_mutex_camera);
			cemu_assert_debug(queue_targetSurfaces.size() < 100); // check for sane queue length
			queue_targetSurfaces.push(*targetSurface);
		}

	private:
		void AcquireHandle()
		{
			std::unique_lock<std::recursive_mutex> _lock(g_mutex_camera);
			for (uint32 i = 0; i < g_table_cameraHandles.size(); i++)
			{
				if (g_table_cameraHandles[i] == nullptr)
				{
					g_table_cameraHandles[i] = this;
					this->handle = i + 1;
					return;
				}
			}
			this->handle = (sint32)(g_table_cameraHandles.size() + 1);
			g_table_cameraHandles.push_back(this);
		}

		void ReleaseHandle()
		{
			for (uint32 i = 0; i < g_table_cameraHandles.size(); i++)
			{
				if (g_table_cameraHandles[i] == this)
				{
					g_table_cameraHandles[i] = nullptr;
					return;
				}
			}
			cemu_assert_debug(false);
		}
	};

	sint32 CAMGetMemReq(void* ukn)
	{
		return 1 * 1024; // always return 1KB
	}

	sint32 CAMCheckMemSegmentation(void* base, uint32 size)
	{
		return CAM_ERROR_SUCCESS; // always return success
	}

	void ppcCAMUpdate60(PPCInterpreter_t* hCPU)
	{
		// update all open camera instances
		size_t numCamInstances = g_activeCameraInstances.size();
		//for (auto& itr : g_activeCameraInstances)
		for(size_t i=0; i<numCamInstances; i++)
		{
			std::unique_lock<std::recursive_mutex> _lock(g_mutex_camera);
			if (i >= g_activeCameraInstances.size())
				break;
			CameraInstance* camInstance = g_activeCameraInstances[i];
			// todo - handle 30 / 60 FPS
			if (camInstance->queue_targetSurfaces.empty())
				continue;
			auto& targetSurface = camInstance->queue_targetSurfaces.front();
			g_cameraHandlerParam->type = 0;
			g_cameraHandlerParam->field1 = targetSurface.surfacePtr.GetMPTR();
			g_cameraHandlerParam->field2 = targetSurface.surfaceSize;
			g_cameraHandlerParam->field3 = 0;
			cemu_assert_debug(camInstance->handlerFunc != MPTR_NULL);
			camInstance->queue_targetSurfaces.pop();
			_lock.unlock();
			PPCCoreCallback(camInstance->handlerFunc, g_cameraHandlerParam.GetPtr());
		}
		osLib_returnFromFunction(hCPU, 0);
	}


	sint32 CAMInit(uint32 cameraId, CAMInitInfo_t* camInitInfo, uint32be* error)
	{
		CameraInstance* camInstance = new CameraInstance(camInitInfo->width, camInitInfo->height, camInitInfo->handlerFuncPtr);

		std::unique_lock<std::recursive_mutex> _lock(g_mutex_camera);
		if (g_cameraCounter == 0)
		{
			coreinit::OSCreateAlarm(g_alarm_camera.GetPtr());
			coreinit::OSSetPeriodicAlarm(g_alarm_camera.GetPtr(), coreinit::coreinit_getOSTime(), (uint64)ESPRESSO_TIMER_CLOCK / 60ull, RPLLoader_MakePPCCallable(ppcCAMUpdate60));
		}
		g_cameraCounter++;

		return camInstance->handle;
	}

	sint32 CAMExit(sint32 camHandle)
	{
		CameraInstance* camInstance = GetCameraInstanceByHandle(camHandle);
		if (!camInstance)
			return CAM_ERROR_INVALID_HANDLE;
		CAMClose(camHandle);
		delete camInstance;

		std::unique_lock<std::recursive_mutex> _lock(g_mutex_camera);
		g_cameraCounter--;
		if (g_cameraCounter == 0)
			coreinit::OSCancelAlarm(g_alarm_camera.GetPtr());
		return CAM_ERROR_SUCCESS;
	}

	sint32 CAMOpen(sint32 camHandle)
	{
		CameraInstance* camInstance = GetCameraInstanceByHandle(camHandle);
		if (!camInstance)
			return CAM_ERROR_INVALID_HANDLE;
		camInstance->OpenCam();
		return CAM_ERROR_SUCCESS;
	}

	sint32 CAMClose(sint32 camHandle)
	{
		CameraInstance* camInstance = GetCameraInstanceByHandle(camHandle);
		if (!camInstance)
			return CAM_ERROR_INVALID_HANDLE;
		camInstance->CloseCam();
		return CAM_ERROR_SUCCESS;
	}

	sint32 CAMSubmitTargetSurface(sint32 camHandle, CAMTargetSurface* targetSurface)
	{
		CameraInstance* camInstance = GetCameraInstanceByHandle(camHandle);
		if (!camInstance)
			return CAM_ERROR_INVALID_HANDLE;
		
		camInstance->QueueTargetSurface(targetSurface);

		return CAM_ERROR_SUCCESS;
	}

	void reset()
	{
		g_cameraCounter = 0;
	}

	void load()
	{
		reset();
		cafeExportRegister("camera", CAMGetMemReq, LogType::Placeholder);
		cafeExportRegister("camera", CAMCheckMemSegmentation, LogType::Placeholder);
		cafeExportRegister("camera", CAMInit, LogType::Placeholder);
		cafeExportRegister("camera", CAMExit, LogType::Placeholder);
		cafeExportRegister("camera", CAMOpen, LogType::Placeholder);
		cafeExportRegister("camera", CAMClose, LogType::Placeholder);
		cafeExportRegister("camera", CAMSubmitTargetSurface, LogType::Placeholder);
	}
}

