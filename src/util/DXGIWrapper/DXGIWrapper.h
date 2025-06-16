#pragma once

#include <dxgi1_4.h>
#include <wrl/client.h>

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

		Microsoft::WRL::ComPtr<IDXGIFactory1> dxgiFactory;
		pCreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

		Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgiAdapter;
		UINT adapterIndex = 0;
		while (dxgiFactory->EnumAdapters1(adapterIndex, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC1 desc;
			dxgiAdapter->GetDesc1(&desc);

			if (deviceLUID == nullptr || memcmp(&desc.AdapterLuid, deviceLUID, sizeof(LUID)) == 0)
			{
				if (FAILED(dxgiAdapter.As(&m_dxgiAdapter)))
				{
					Cleanup();
					throw std::runtime_error("can't create dxgi adapter");
				}
				break;
			}

			++adapterIndex;
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
	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_dxgiAdapter;

	void Cleanup()
	{
		m_dxgiAdapter.Reset();

		if (m_moduleHandle)
		{
			FreeLibrary(m_moduleHandle);
			m_moduleHandle = nullptr;
		}
	}
};
