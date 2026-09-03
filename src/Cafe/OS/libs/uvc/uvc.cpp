#include "uvc.h"

namespace uvc
{
	class : public COSModule {
		public:
		std::string_view GetName() override
		{
			return "uvc";
		}
		std::vector<std::string_view> GetDependencies() override
		{
			return {"coreinit"};
		}
	} s_COSuvcModule;

	COSModule* GetModule()
	{
		return &s_COSuvcModule;
	}
} // namespace uvc