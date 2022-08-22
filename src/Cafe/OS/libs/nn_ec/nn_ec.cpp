#include "Cafe/OS/common/OSCommon.h"

typedef struct  
{
	/* +0x00 */ uint8 ukn00[0x10];
	/* +0x10 */ uint8 ukn10[0x0C];
	/* +0x2C */ uint8 ukn2C[0x10]; // currency string?
	/* +0x3C */ uint32 ukn3C; // money amount?
	// size of struct is 0x40
}nnEcUknMoneyStruct_t;

void nnEcExport___ct__Q3_2nn2ec5MoneyFPCcN21(PPCInterpreter_t* hCPU)
{
	debug_printf("nn_ec.__ct__Q3_2nn2ec5MoneyFPCcN21(0x%08x, ...)\n", hCPU->gpr[3]);
	nnEcUknMoneyStruct_t* moneyStruct = (nnEcUknMoneyStruct_t*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
	// todo -> Setup struct
	osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(moneyStruct));
}

/*
 * Load E commerce functions (E-Shop stuff)
 */
void nnEc_load()
{
	osLib_addFunction("nn_ec", "__ct__Q3_2nn2ec5MoneyFPCcN21", nnEcExport___ct__Q3_2nn2ec5MoneyFPCcN21);
}