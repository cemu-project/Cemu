#include "Cafe/OS/common/OSCommon.h"

typedef struct  
{
	uint32 reserved;
}udsWorkspace_t;

udsWorkspace_t* udsWorkspace = nullptr;

void nnUdsExport___sti___11_uds_Api_cpp_f5d9abb2(PPCInterpreter_t* hCPU)
{
	debug_printf("__sti___11_uds_Api_cpp_f5d9abb2()\n");
	if( udsWorkspace == NULL )
		udsWorkspace = (udsWorkspace_t*)memory_getPointerFromVirtualOffset(coreinit_allocFromSysArea(32, 32));
	osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(udsWorkspace));
}

namespace nn::uds
{
	class : public COSModule
	{
		public:
		std::string_view GetName() override
		{
			return "nn_uds";
		}

		void RPLMapped() override
		{
			osLib_addFunction("nn_uds", "__sti___11_uds_Api_cpp_f5d9abb2", nnUdsExport___sti___11_uds_Api_cpp_f5d9abb2);
		};

		void rpl_entry(uint32 moduleHandle, coreinit::RplEntryReason reason) override
		{
			if (reason == coreinit::RplEntryReason::Loaded)
			{
				udsWorkspace = nullptr;
			}
			else if (reason == coreinit::RplEntryReason::Unloaded)
			{
				udsWorkspace = nullptr;
			}
		}
	}s_COSnnUdsModule;

	COSModule* GetModule()
	{
		return &s_COSnnUdsModule;
	}
}