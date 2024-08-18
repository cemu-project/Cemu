#pragma once
#include "Cafe/IOSU/legacy/iosu_acp.h"

namespace nn
{
namespace acp
{
	enum ACPStatus : uint32
	{
		SUCCESS = 0,
	};

	using ACPDeviceType = iosu::acp::ACPDeviceType;

	ACPStatus ACPGetApplicationBox(uint32be* applicationBox, uint64 titleId);
	ACPStatus ACPMountSaveDir();
    ACPStatus ACPUnmountSaveDir();
	ACPStatus ACPCreateSaveDir(uint32 persistentId, iosu::acp::ACPDeviceType type);
	ACPStatus ACPUpdateSaveTimeStamp(uint32 persistentId, uint64 titleId, iosu::acp::ACPDeviceType deviceType);

	void load();
}
}
