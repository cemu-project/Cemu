#pragma once

#include <dxgi1_4.h>
//#include <atlbase.h>

class DXGIWrapper
{
public:
	DXGIWrapper()
		: DXGIWrapper(nullptr)
	{}
	
	DXGIWrapper(uint8* deviceLUID)
	{
		m_moduleHandle = LoadLibraryA("dxgi.dll");
		if (!m_moduleHandle)
			throw std::runtime_error("can't load dxgi module");

		const auto pCreateDXGIFactory1 = (decltype(&CreateDXGIFactory1))GetProcAddress(m_moduleHandle, "CreateDXGIFactory1");
		if (!pCreateDXGIFactory1)
		{
			FreeLibrary(m_moduleHandle);
			throw std::runtime_error("can't find CreateDXGIFactory1 in dxgi module");
		}

		IDXGIFactory1* dxgiFactory = nullptr;
		pCreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

		IDXGIAdapter1* tmpDxgiAdapter = nullptr;
		UINT adapterIndex = 0;
		while (dxgiFactory->EnumAdapters1(adapterIndex, &tmpDxgiAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC1 desc;
			tmpDxgiAdapter->GetDesc1(&desc);

			if (deviceLUID == nullptr || memcmp(&desc.AdapterLuid, deviceLUID, sizeof(LUID)) == 0)
			{
				tmpDxgiAdapter->QueryInterface(IID_PPV_ARGS(&m_dxgiAdapter));
				tmpDxgiAdapter->Release();
				break;
			}

			tmpDxgiAdapter->Release();
			++adapterIndex;
		}

		dxgiFactory->Release();

		if (!m_dxgiAdapter)
		{
			Cleanup();
			throw std::runtime_error("can't create dxgi adapter");
		}
	}

	~DXGIWrapper()
	{
		Cleanup();
	}

	bool QueryVideoMemoryInfo(DXGI_QUERY_VIDEO_MEMORY_INFO& info) const
	{
		return m_dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info) == S_OK;
	}

private:
	HMODULE m_moduleHandle = nullptr;
	IDXGIAdapter3* m_dxgiAdapter = nullptr;

	void Cleanup()
	{
		if (m_dxgiAdapter)
		{
			m_dxgiAdapter->Release();
			m_dxgiAdapter = nullptr;
		}

		if (m_moduleHandle)
		{
			FreeLibrary(m_moduleHandle);
			m_moduleHandle = nullptr;
		}
	}
};