#include "napi.h"
#include "config/CemuConfig.h"
#include "config/NetworkSettings.h"
#include "config/ActiveSettings.h"

namespace NAPI
{
	NetworkService AuthInfo::GetService() const
	{
		return serviceOverwrite.value_or(ActiveSettings::GetNetworkService());
	}
}
