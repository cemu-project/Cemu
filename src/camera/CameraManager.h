#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <openpnp-capture.h>
#include "util/helpers/Singleton.h"

class CameraManager : public Singleton<CameraManager>
{
	CapContext m_ctx;
	std::optional<CapDeviceID> m_device;
	std::optional<CapStream> m_stream;
	std::vector<uint8> m_rgbBuffer;
	std::vector<uint8> m_nv12Buffer;
	int m_refCount;
	std::thread m_captureThread;
	std::atomic_bool m_capturing;
	std::atomic_bool m_running;
	mutable std::recursive_mutex m_mutex;

  public:
	constexpr static uint32 DEVICE_NONE = std::numeric_limits<uint32>::max();
	struct DeviceInfo
	{
		std::string uniqueId;
		std::string name;
	};
	CameraManager();
	~CameraManager();

	void SetDevice(uint32 deviceNo);
	std::vector<DeviceInfo> EnumerateDevices();
	void SaveDevice();

	void Open();
	void Close();
	void FillNV12Buffer(uint8* nv12Buffer) const;
	void FillRGBBuffer(uint8* rgbBuffer) const;

  private:
	std::optional<CapFormatID> FindCorrectFormat();
	void ResetBuffers();
	void CaptureWorker();
	void OpenStream();
	void CloseStream();
};
