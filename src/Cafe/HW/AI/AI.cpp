#include "Cafe/HW/MMU/MMU.h"


namespace HW_AI
{

	void AI_STATUS_W16(uint32 addr, uint16 value)
	{

	}

	RunAtCemuBoot _init([]()
	{
		//using MMIOFuncWrite16 = void (*)(uint32 addr, uint16 value);
		//using MMIOFuncWrite32 = void (*)(uint32 addr, uint32 value);

		//void RegisterMMIO_W16(MMIOFuncWrite16 ptr);
	});
}
