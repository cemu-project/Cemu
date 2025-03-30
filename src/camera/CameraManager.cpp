#include "CameraManager.h"
#include "Rgb2Nv12.h"
#include "config/CemuConfig.h"
#include "util/helpers/helpers.h"

constexpr unsigned CAMERA_WIDTH = 640;
constexpr unsigned CAMERA_HEIGHT = 480;
constexpr unsigned CAMERA_PITCH = 768;

CameraManager::CameraManager()
	: m_ctx(Cap_createContext()),
	  m_rgbBuffer(CAMERA_WIDTH * CAMERA_HEIGHT * 3),
	  m_nv12Buffer(CAMERA_PITCH * CAMERA_HEIGHT * 3 / 2),
	  m_refCount(0), m_capturing(false), m_running(true)
{
	m_captureThread = std::thread(&CameraManager::CaptureWorker, this);

	const auto uniqueId = GetConfig().camera_id.GetValue();
	if (!uniqueId.empty())
	{
		const auto devices = EnumerateDevices();
		for (CapDeviceID deviceId = 0; deviceId < devices.size(); ++deviceId)
		{
			if (devices[deviceId].uniqueId == uniqueId)
			{
				m_device = deviceId;
				return;
			}
		}
	}
	ResetBuffers();
}
CameraManager::~CameraManager()
{
	m_running = false;
	CloseStream();
	Cap_releaseContext(m_ctx);
}

void CameraManager::SetDevice(uint32 deviceNo)
{
	std::scoped_lock lock(m_mutex);
	CloseStream();
	if (deviceNo == DEVICE_NONE)
	{
		m_device = std::nullopt;
		ResetBuffers();
		return;
	}
	m_device = deviceNo;
	if (m_refCount != 0)
		OpenStream();
}
std::vector<CameraManager::DeviceInfo> CameraManager::EnumerateDevices()
{
	std::scoped_lock lock(m_mutex);
	std::vector<DeviceInfo> infos;
	const auto deviceCount = Cap_getDeviceCount(m_ctx);
	for (CapDeviceID deviceNo = 0; deviceNo < deviceCount; ++deviceNo)
	{
		const auto uniqueId = Cap_getDeviceUniqueID(m_ctx, deviceNo);
		const auto name = Cap_getDeviceName(m_ctx, deviceNo);

		if (name)
			infos.emplace_back(DeviceInfo{uniqueId, fmt::format("{}: {}", deviceNo + 1, name)});
		else
			infos.emplace_back(DeviceInfo(uniqueId, fmt::format("{}: Unknown", deviceNo + 1)));
	}
	return infos;
}
void CameraManager::SaveDevice()
{
	std::scoped_lock lock(m_mutex);
	if (m_device)
		GetConfig().camera_id = Cap_getDeviceUniqueID(m_ctx, *m_device);
	else
		GetConfig().camera_id = "";
}
void CameraManager::Open()
{
	std::scoped_lock lock(m_mutex);
	if (m_device && m_refCount == 0)
	{
		OpenStream();
	}
	m_refCount += 1;
}
void CameraManager::Close()
{
	std::scoped_lock lock(m_mutex);
	if (m_refCount == 0)
		return;
	m_refCount -= 1;
	if (m_refCount != 0)
		return;
	CloseStream();
}

std::optional<CapFormatID> CameraManager::FindCorrectFormat()
{
	const auto formatCount = Cap_getNumFormats(m_ctx, *m_device);
	for (CapFormatID formatId = 0; formatId < formatCount; ++formatId)
	{
		CapFormatInfo formatInfo;
		if (Cap_getFormatInfo(m_ctx, *m_device, formatId, &formatInfo) != CAPRESULT_OK)
			continue;
		if (formatInfo.width == CAMERA_WIDTH && formatInfo.height == CAMERA_HEIGHT)
			return formatId;
	}
	return std::nullopt;
}
void CameraManager::ResetBuffers()
{
	std::ranges::fill(m_rgbBuffer, 0);
	std::ranges::fill_n(m_nv12Buffer.begin(), CAMERA_WIDTH * CAMERA_PITCH, 16);
	std::ranges::fill_n(m_nv12Buffer.begin() + CAMERA_WIDTH * CAMERA_PITCH, (CAMERA_WIDTH / 2), 128);
}

void CameraManager::FillNV12Buffer(uint8* nv12Buffer) const
{
	std::scoped_lock lock(m_mutex);
	std::ranges::copy(m_nv12Buffer, nv12Buffer);
}

void CameraManager::FillRGBBuffer(uint8* rgbBuffer) const
{
	std::scoped_lock lock(m_mutex);
	std::ranges::copy(m_rgbBuffer, rgbBuffer);
}
void CameraManager::CaptureWorker()
{
	SetThreadName("CameraManager");
	while (m_running)
	{
		while (m_capturing)
		{
			m_mutex.lock();
			if (m_stream && Cap_hasNewFrame(m_ctx, *m_stream) &&
				Cap_captureFrame(m_ctx, *m_stream, m_rgbBuffer.data(), m_rgbBuffer.size()) == CAPRESULT_OK)
				Rgb2Nv12(m_rgbBuffer.data(), CAMERA_WIDTH, CAMERA_HEIGHT, m_nv12Buffer.data(), CAMERA_PITCH);
			m_mutex.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::this_thread::yield();
	}
}
void CameraManager::OpenStream()
{
	const auto formatId = FindCorrectFormat();
	if (!formatId)
		return;
	const auto stream = Cap_openStream(m_ctx, *m_device, *formatId);
	if (stream == -1)
		return;
	m_capturing = true;
	m_stream = stream;
}
void CameraManager::CloseStream()
{
	m_capturing = false;
	if (m_stream)
	{
		Cap_closeStream(m_ctx, *m_stream);
		m_stream = std::nullopt;
	}
}