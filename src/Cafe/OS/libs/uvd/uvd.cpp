#include "uvd.h"

namespace uvd
{
	class : public COSModule
	{
	  public:
		std::string_view GetName() override
		{
			return "uvd";
		}
		std::vector<std::string_view> GetDependencies() override
		{
			return {"coreinit", "tcl"};
		}
	} s_COSuvdModule;

	COSModule* GetModule()
	{
		return &s_COSuvdModule;
	}
} // namespace uvd