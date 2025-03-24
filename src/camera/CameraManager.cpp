#include "CameraManager.h"
#include "Rgb2Nv12.h"
#include "util/helpers/helpers.h"

constexpr unsigned CAMERA_WIDTH = 640;
constexpr unsigned CAMERA_HEIGHT = 480;
constexpr unsigned CAMERA_PITCH = 768;

CameraManager::CameraManager()
	: m_ctx(Cap_createContext()),
	  m_rgbBuffer(CAMERA_WIDTH * CAMERA_HEIGHT * 3),
	  m_nv12Buffer(CAMERA_PITCH * CAMERA_HEIGHT * 3 / 2),
	  m_refCount(0)
{
	if (Cap_getDeviceCount(m_ctx) > 0)
		m_device = 0;
}
CameraManager::~CameraManager()
{
	Close();
	Cap_releaseContext(m_ctx);
}

void CameraManager::SetDevice(unsigned deviceNo)
{
	std::scoped_lock lock(m_mutex);
	if (m_device == deviceNo)
		return;
	if (m_stream)
		Cap_closeStream(m_ctx, *m_stream);
	m_device = deviceNo;
}
bool CameraManager::Open(bool weak)
{
	std::scoped_lock lock(m_mutex);
	if (!m_device)
		return false;
	if (m_refCount == 0)
	{
		const auto formatCount = Cap_getNumFormats(m_ctx, *m_device);
		CapFormatID formatId = 0;
		for (; formatId < formatCount; ++formatId)
		{
			CapFormatInfo formatInfo;
			if (Cap_getFormatInfo(m_ctx, *m_device, formatId, &formatInfo) != CAPRESULT_OK)
				continue;
			if (formatInfo.width == CAMERA_WIDTH && formatInfo.height == CAMERA_HEIGHT)
				break;
		}
		if (formatId == formatCount)
			return false;
		auto stream = Cap_openStream(m_ctx, *m_device, formatId);
		if (stream == -1)
			return false;
		m_capturing = true;
		m_stream = stream;
		m_captureThread = std::thread(&CameraManager::CaptureWorker, this);
	}
	if (!weak)
		m_refCount += 1;
	return true;
}
void CameraManager::Close()
{
	{
		std::scoped_lock lock(m_mutex);
		if (m_refCount == 0)
			return;
		m_refCount -= 1;
		if (m_refCount != 0)
			return;
		Cap_closeStream(m_ctx, *m_stream);
		m_stream = std::nullopt;
		m_capturing = false;
	}
	m_captureThread.join();
}

void CameraManager::FillNV12Buffer(uint8* nv12Buffer) const
{
	std::shared_lock lock(m_mutex);
	std::ranges::copy(m_nv12Buffer, nv12Buffer);
}
void CameraManager::CaptureWorker()
{
	SetThreadName("CameraManager");
	while (m_capturing)
	{
		{
			std::scoped_lock lock(m_mutex);
			bool frameAvailable = Cap_hasNewFrame(m_ctx, *m_stream);
			if (frameAvailable &&
				Cap_captureFrame(m_ctx, *m_stream, m_rgbBuffer.data(), m_rgbBuffer.size()) == CAPRESULT_OK)
			{
				Rgb2Nv12(m_rgbBuffer.data(), CAMERA_WIDTH, CAMERA_HEIGHT, m_nv12Buffer.data(), CAMERA_PITCH);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
		std::this_thread::yield();
	}
}