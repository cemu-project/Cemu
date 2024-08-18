
namespace proc_ui
{
	enum class ProcUIStatus
	{
		Foreground = 0,
		Background = 1,
		Releasing = 2,
		Exit = 3
	};

	enum class ProcUICallbackId
	{
		AcquireForeground = 0,
		ReleaseForeground = 1,
		Exit = 2,
		NetIoStart = 3,
		NetIoStop = 4,
		HomeButtonDenied = 5,
		COUNT = 6
	};

	void ProcUIInit(MEMPTR<void> callbackReadyToRelease);
	void ProcUIInitEx(MEMPTR<void> callbackReadyToReleaseEx, MEMPTR<void> userParam);
	void ProcUIShutdown();
	bool ProcUIIsRunning();
	bool ProcUIInForeground();
	bool ProcUIInShutdown();
	void ProcUIRegisterCallback(ProcUICallbackId callbackType, void* funcPtr, void* userParam, sint32 priority);
	void ProcUIRegisterCallbackCore(ProcUICallbackId callbackType, void* funcPtr, void* userParam, sint32 priority, uint32 coreIndex);
	void ProcUIRegisterBackgroundCallback(void* funcPtr, void* userParam, uint64 tickDelay);
	void ProcUIClearCallbacks();
	void ProcUISetSaveCallback(void* funcPtr, void* userParam);
	void ProcUISetCallbackStackSize(uint32 newStackSize);
	uint32 ProcUICalcMemorySize(uint32 numCallbacks);
	sint32 ProcUISetMemoryPool(void* memBase, uint32 size);
	void ProcUISetBucketStorage(void* memBase, uint32 size);
	void ProcUISetMEM1Storage(void* memBase, uint32 size);
	void ProcUIDrawDoneRelease();
	ProcUIStatus ProcUIProcessMessages(bool isBlockingInBackground);
	ProcUIStatus ProcUISubProcessMessages(bool isBlockingInBackground);

	void load();
}