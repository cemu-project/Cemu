#pragma once

class VsyncDriver {
  public:
	VsyncDriver(void(*cbVSync)());
	virtual ~VsyncDriver() = default;

  protected:
	void startThread();
	void signalVsync();
	void setCallback(void (*cbVSync)());

	virtual void vsyncThread() = 0;
	std::jthread m_thd;
	void (*m_vsyncDriverVSyncCb)() = nullptr;
};
class VsyncDriverDXGI;

extern std::unique_ptr<VsyncDriver> g_vsyncDriver;

void VsyncDriver_startThread(void(*cbVSync)());
void VsyncDriver_notifyWindowPosChanged();