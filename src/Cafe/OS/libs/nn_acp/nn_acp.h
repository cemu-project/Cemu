#pragma once

namespace nn
{
namespace acp
{
	enum ACPStatus : uint32
	{
		SUCCESS = 0,
	};

	enum ACPDeviceType
	{
		UnknownType = 0,
		InternalDeviceType = 1,
		USBDeviceType = 3,
	};

	void CreateSaveMetaFiles(uint32 persistentId, uint64 titleId);

	ACPStatus ACPGetApplicationBox(uint32be* applicationBox, uint64 titleId);
	ACPStatus ACPMountSaveDir();
    ACPStatus ACPUnmountSaveDir();
	ACPStatus ACPCreateSaveDir(uint32 persistentId, ACPDeviceType type);
	ACPStatus ACPUpdateSaveTimeStamp(uint32 persistentId, uint64 titleId, ACPDeviceType deviceType);;

	void load();
}
}
