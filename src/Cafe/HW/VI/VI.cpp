#include "Cafe/HW/MMU/MMU.h"


namespace HW_VI
{

	RunAtCemuBoot _initVI([]()
	{
		//MMU::RegisterMMIO_R16(MMU::MMIOInterface::INTERFACE_0C000000, 0x1e0002, VI_UKN1E0002_R16);
	});
}
