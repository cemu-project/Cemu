#pragma once
#include <shared_mutex>
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
	mutable std::shared_mutex m_mutex;

  public:
	CameraManager();
	~CameraManager();

	void SetDevice(uint32 deviceNo);

	bool Open(bool weak);
	void Close();

	void FillNV12Buffer(uint8* nv12Buffer) const;

  private:
	void CaptureWorker();
};