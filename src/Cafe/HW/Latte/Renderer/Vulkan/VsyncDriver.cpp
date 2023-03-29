#include "gui/MainWindow.h"

#if BOOST_OS_WINDOWS

#include <Windows.h>

typedef LONG NTSTATUS;

typedef UINT32 D3DKMT_HANDLE;
typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;

typedef struct _D3DKMT_OPENADAPTERFROMHDC
{
	HDC                            hDc;
	D3DKMT_HANDLE                  hAdapter;
	LUID                           AdapterLuid;
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
}D3DKMT_OPENADAPTERFROMHDC;

typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT {
	D3DKMT_HANDLE                  hAdapter;
	D3DKMT_HANDLE                  hDevice;
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} D3DKMT_WAITFORVERTICALBLANKEVENT;

class DeviceVsyncHandler
{
public:
	DeviceVsyncHandler(void(*cbVSync)()) : m_vsyncDriverVSyncCb(cbVSync)
	{
		m_shutdownThread = false;
		if (!pfnD3DKMTOpenAdapterFromHdc)
		{
			HMODULE hModuleGDI = LoadLibraryA("gdi32.dll");
			*(void**)&pfnD3DKMTOpenAdapterFromHdc = GetProcAddress(hModuleGDI, "D3DKMTOpenAdapterFromHdc");
			*(void**)&pfnD3DKMTWaitForVerticalBlankEvent = GetProcAddress(hModuleGDI, "D3DKMTWaitForVerticalBlankEvent");
		}
		m_thd = std::thread(&DeviceVsyncHandler::vsyncThread, this);
	}

	~DeviceVsyncHandler()
	{
		m_shutdownThread = true;
		cemu_assert_debug(m_thd.joinable());
		m_thd.join();
	}

	void notifyWindowPosChanged()
	{
		m_checkMonitorChange = true;
	}

private:
	bool HasMonitorChanged()
	{
		HWND hWnd = (HWND)g_mainFrame->GetRenderCanvasHWND();
		if (hWnd == 0)
			return true;
		HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

		MONITORINFOEXW monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(hMonitor, &monitorInfo) == 0)
			return true;

		if (wcscmp(monitorInfo.szDevice, m_activeMonitorDevice) == 0)
			return false;
		
		return true;
	}

	HRESULT GetAdapterHandleFromHwnd(D3DKMT_HANDLE* phAdapter, UINT* pOutput)
	{
		if (!g_mainFrame)
			return E_FAIL;
		HWND hWnd = (HWND)g_mainFrame->GetRenderCanvasHWND();
		if (hWnd == 0)
			return E_FAIL;

		wcsncpy(m_activeMonitorDevice, L"", 32); // reset remembered monitor device
		m_checkMonitorChange = false;

		D3DKMT_OPENADAPTERFROMHDC OpenAdapterData;

		*phAdapter = NULL;
		*pOutput = 0;

		HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

		MONITORINFOEXW monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(hMonitor, &monitorInfo) == 0)
			return E_FAIL;


		HDC hdc = CreateDCW(NULL, monitorInfo.szDevice, NULL, NULL);
		if (hdc == NULL) {
			return E_FAIL;
		}

		OpenAdapterData.hDc = hdc;
		if (pfnD3DKMTOpenAdapterFromHdc(&OpenAdapterData) == 0)
		{
			DeleteDC(hdc);
			*phAdapter = OpenAdapterData.hAdapter;
			*pOutput = OpenAdapterData.VidPnSourceId;
			// remember monitor device
			wcsncpy(m_activeMonitorDevice, monitorInfo.szDevice, 32);
			return S_OK;
		}
		DeleteDC(hdc);

		return E_FAIL;
	}

	void vsyncThread()
	{
		D3DKMT_HANDLE hAdapter;
		UINT hOutput;
		GetAdapterHandleFromHwnd(&hAdapter, &hOutput);

		int failCount = 0;
		while (!m_shutdownThread)
		{
			D3DKMT_WAITFORVERTICALBLANKEVENT arg;
			arg.hDevice = 0;
			arg.hAdapter = hAdapter;
			arg.VidPnSourceId = hOutput;
			NTSTATUS r = pfnD3DKMTWaitForVerticalBlankEvent(&arg);
			if (r != 0)
			{
				//cemuLog_log(LogType::Force, "Wait for VerticalBlank failed");
				Sleep(1000 / 60);
				failCount++;
				if (failCount >= 10)
				{
					while (GetAdapterHandleFromHwnd(&hAdapter, &hOutput) != S_OK)
					{
						Sleep(1000 / 60);
						if (m_shutdownThread)
							return;
					}
					failCount = 0;
				}
			}
			else
				signalVsync();
			if (m_checkMonitorChange)
			{
				m_checkMonitorChange = false;
				if (HasMonitorChanged())
				{
					while (GetAdapterHandleFromHwnd(&hAdapter, &hOutput) != S_OK)
					{
						Sleep(1000 / 60);
						if (m_shutdownThread)
							return;
					}
				}
			}
		}
	}

	void signalVsync()
	{
		if(m_vsyncDriverVSyncCb)
			m_vsyncDriverVSyncCb();
	}

	void setCallback(void(*cbVSync)())
	{
		m_vsyncDriverVSyncCb = cbVSync;
	}

private:
	NTSTATUS(__stdcall* pfnD3DKMTOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC* Arg1) = nullptr;
	NTSTATUS(__stdcall* pfnD3DKMTWaitForVerticalBlankEvent)(const D3DKMT_WAITFORVERTICALBLANKEVENT* Arg1) = nullptr;

	std::thread m_thd;
	bool m_shutdownThread;
	bool m_checkMonitorChange{};
	WCHAR m_activeMonitorDevice[32];

	void (*m_vsyncDriverVSyncCb)() = nullptr;
};

DeviceVsyncHandler* s_vsyncDriver = nullptr;

std::mutex s_driverAccess;

void VsyncDriver_startThread(void(*cbVSync)())
{
	std::unique_lock<std::mutex> ul(s_driverAccess);
	if (!s_vsyncDriver)
		s_vsyncDriver = new DeviceVsyncHandler(cbVSync);
}

void VsyncDriver_notifyWindowPosChanged()
{
	std::unique_lock<std::mutex> ul(s_driverAccess);
	if (s_vsyncDriver)
		s_vsyncDriver->notifyWindowPosChanged();
}

#else

void VsyncDriver_startThread(void(*cbVSync)())
{
	cemu_assert_unimplemented();
}

void VsyncDriver_notifyWindowPosChanged()
{

}

#endif
