void nnAc_load();
#include "Cafe/OS/RPL/COSModule.h"

namespace nn_ac
{
	nnResult IsApplicationConnected(uint8be* connected);
}

namespace nn::ac
{
	COSModule* GetModule();
}