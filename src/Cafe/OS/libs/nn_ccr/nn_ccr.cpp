#include "Cafe/OS/common/OSCommon.h"
#include "nn_ccr.h"

namespace nn::ccr
{
	sint32 CCRSysCaffeineBootCheck()
	{
		cemuLog_logDebug(LogType::Force, "CCRSysCaffeineBootCheck()");
		return -1;
	}

	class : public COSModule
	{
		public:
		std::string_view GetName() override
		{
			return "nn_ccr";
		}

		void RPLMapped() override
		{
			cafeExportRegister("nn_ccr", CCRSysCaffeineBootCheck, LogType::Placeholder);
		};

	}s_COSnnccrModule;

	COSModule* GetModule()
	{
		return &s_COSnnccrModule;
	}
}
