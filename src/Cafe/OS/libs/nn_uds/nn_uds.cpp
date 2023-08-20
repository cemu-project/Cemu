#include "Cafe/OS/common/OSCommon.h"

typedef struct  
{
	uint32 reserved;
}udsWorkspace_t;

udsWorkspace_t* udsWorkspace = NULL;

void nnUdsExport___sti___11_uds_Api_cpp_f5d9abb2(PPCInterpreter_t* hCPU)
{
	debug_printf("__sti___11_uds_Api_cpp_f5d9abb2()\n");
	if( udsWorkspace == NULL )
		udsWorkspace = (udsWorkspace_t*)memory_getPointerFromVirtualOffset(coreinit_allocFromSysArea(32, 32));
	osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(udsWorkspace));
}

/*
 * Load UDS functions
 */
void nnUds_load()
{
	osLib_addFunction("nn_uds", "__sti___11_uds_Api_cpp_f5d9abb2", nnUdsExport___sti___11_uds_Api_cpp_f5d9abb2);
}
