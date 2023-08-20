#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/OS/libs/proc_ui/proc_ui.h"
#include "Cafe/OS/libs/nsysnet/nsysnet.h"
#include "Cafe/OS/libs/nlibnss/nlibnss.h"
#include "Cafe/OS/libs/nlibcurl/nlibcurl.h"
#include "Cafe/OS/libs/nn_nfp/nn_nfp.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"
#include "Cafe/OS/libs/nn_acp/nn_acp.h"
#include "Cafe/OS/libs/nn_ac/nn_ac.h"
#include "Cafe/OS/libs/nn_uds/nn_uds.h"
#include "Cafe/OS/libs/nn_nim/nn_nim.h"
#include "Cafe/OS/libs/nn_ndm/nn_ndm.h"
#include "Cafe/OS/libs/nn_spm/nn_spm.h"
#include "Cafe/OS/libs/nn_ec/nn_ec.h"
#include "Cafe/OS/libs/nn_boss/nn_boss.h"
#include "Cafe/OS/libs/nn_fp/nn_fp.h"
#include "Cafe/OS/libs/nn_olv/nn_olv.h"
#include "Cafe/OS/libs/nn_idbe/nn_idbe.h"
#include "Cafe/OS/libs/nn_save/nn_save.h"
#include "Cafe/OS/libs/erreula/erreula.h"
#include "Cafe/OS/libs/sysapp/sysapp.h"
#include "Cafe/OS/libs/dmae/dmae.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/OS/libs/vpad/vpad.h"
#include "Cafe/OS/libs/nsyskbd/nsyskbd.h"
#include "Cafe/OS/libs/nsyshid/nsyshid.h"
#include "Cafe/OS/libs/snd_user/snd_user.h"
#include "Cafe/OS/libs/zlib125/zlib125.h"
#include "Cafe/OS/libs/padscore/padscore.h"
#include "Cafe/OS/libs/camera/camera.h"
#include "../libs/swkbd/swkbd.h"

struct osFunctionEntry_t
{
	uint32 libHashA;
	uint32 libHashB;
	uint32 funcHashA;
	uint32 funcHashB;
	std::string name;
	HLEIDX hleFunc;

	osFunctionEntry_t(uint32 libHashA, uint32 libHashB, uint32 funcHashA, uint32 funcHashB, std::string_view name, HLEIDX hleFunc) :
		libHashA(libHashA), libHashB(libHashB), funcHashA(funcHashA), funcHashB(funcHashB), name(name), hleFunc(hleFunc) {};
};

typedef struct
{
	uint32 libHashA;
	uint32 libHashB;
	uint32 funcHashA;
	uint32 funcHashB;
	uint32 vPtr;
}osPointerEntry_t;

std::vector<osFunctionEntry_t>* s_osFunctionTable;
std::vector<osPointerEntry_t> osDataTable;

void osLib_generateHashFromName(const char* name, uint32* hashA, uint32* hashB)
{
	uint32 h1 = 0x688BA2BA;
	uint32 h2 = 0xF64A71D5;
	while( *name )
	{
		uint32 c = (uint32)*name;
		h1 += c;
		h1 = (h1<<3)|((h1>>29));
		h2 ^= c;
		h2 = (h2<<7)|((h2>>25));
		h1 += h2;
		h2 += c;
		h2 = (h2<<3)|((h2>>29));
		name++;
	}
	*hashA = h1;
	*hashB = h2;
}

void osLib_addFunctionInternal(const char* libraryName, const char* functionName, void(*osFunction)(PPCInterpreter_t* hCPU))
{
	if (!s_osFunctionTable)
		s_osFunctionTable = new std::vector<osFunctionEntry_t>(); // replace with static allocation + constinit once we have C++20 available
	// calculate hash
	uint32 libHashA, libHashB;
	uint32 funcHashA, funcHashB;
	osLib_generateHashFromName(libraryName, &libHashA, &libHashB);
	osLib_generateHashFromName(functionName, &funcHashA, &funcHashB);
	// if entry already exists, update it
	for (auto& it : *s_osFunctionTable)
	{
		if (it.libHashA == libHashA &&
			it.libHashB == libHashB &&
			it.funcHashA == funcHashA &&
			it.funcHashB == funcHashB)
		{
			it.hleFunc = PPCInterpreter_registerHLECall(osFunction);
			return;
		}
	}
	s_osFunctionTable->emplace_back(libHashA, libHashB, funcHashA, funcHashB, fmt::format("{}.{}", libraryName, functionName), PPCInterpreter_registerHLECall(osFunction));
}

extern "C" DLLEXPORT void osLib_registerHLEFunction(const char* libraryName, const char* functionName, void(*osFunction)(PPCInterpreter_t * hCPU))
{
	osLib_addFunctionInternal(libraryName, functionName, osFunction);
}

sint32 osLib_getFunctionIndex(const char* libraryName, const char* functionName)
{
	uint32 libHashA, libHashB;
	uint32 funcHashA, funcHashB;
	osLib_generateHashFromName(libraryName, &libHashA, &libHashB);
	osLib_generateHashFromName(functionName, &funcHashA, &funcHashB);
	for (auto& it : *s_osFunctionTable)
	{
		if (it.libHashA == libHashA &&
			it.libHashB == libHashB &&
			it.funcHashA == funcHashA &&
			it.funcHashB == funcHashB)
		{
			return it.hleFunc;
		}
	}
	return -1;
}

void osLib_addVirtualPointer(const char* libraryName, const char* functionName, uint32 vPtr)
{
	// calculate hash
	uint32 libHashA, libHashB;
	uint32 funcHashA, funcHashB;
	osLib_generateHashFromName(libraryName, &libHashA, &libHashB);
	osLib_generateHashFromName(functionName, &funcHashA, &funcHashB);
	// if entry already exists, update it
	for (auto& it : osDataTable)
	{
		if (it.libHashA == libHashA &&
			it.libHashB == libHashB &&
			it.funcHashA == funcHashA &&
			it.funcHashB == funcHashB)
		{
			it.vPtr = vPtr;
			return;
		}
	}
	// add entry
	auto writeIndex = osDataTable.size();
	osDataTable.resize(osDataTable.size() + 1);
	osDataTable[writeIndex].libHashA = libHashA;
	osDataTable[writeIndex].libHashB = libHashB;
	osDataTable[writeIndex].funcHashA = funcHashA;
	osDataTable[writeIndex].funcHashB = funcHashB;
	osDataTable[writeIndex].vPtr = vPtr;
}

uint32 osLib_getPointer(const char* libraryName, const char* functionName)
{
	uint32 libHashA, libHashB;
	uint32 funcHashA, funcHashB;
	osLib_generateHashFromName(libraryName, &libHashA, &libHashB);
	osLib_generateHashFromName(functionName, &funcHashA, &funcHashB);
	for (auto& it : osDataTable)
	{
		if (it.libHashA == libHashA &&
			it.libHashB == libHashB &&
			it.funcHashA == funcHashA &&
			it.funcHashB == funcHashB)
		{
			return it.vPtr;
		}
	}
	return 0xFFFFFFFF;
}

void osLib_returnFromFunction(PPCInterpreter_t* hCPU, uint32 returnValue)
{
	hCPU->gpr[3] = returnValue;
	hCPU->instructionPointer = hCPU->spr.LR;
}

void osLib_returnFromFunction64(PPCInterpreter_t* hCPU, uint64 returnValue64)
{
	hCPU->gpr[3] = (returnValue64>>32)&0xFFFFFFFF;
	hCPU->gpr[4] = (returnValue64>>0)&0xFFFFFFFF;
	hCPU->instructionPointer = hCPU->spr.LR;
}

void osLib_load()
{
	// load HLE modules
	coreinit_load();
	zlib::load();
	gx2_load();
	dmae_load();
	padscore::load();
	vpad::load();
	snd_core::loadExports();
	nn::erreula::load();
	nnAct_load();
	nn::acp::load();
	nnAc_load();
	nnEc_load();
	nnBoss_load();
	nn::nfp::load();
	nnUds_load();
	nn::nim::load();
	nn::ndm::load();
	nn::spm::load();
	nn::save::load();
	nsysnet_load();
	nn::fp::load();
	nn::olv::load();
	nn::idbe::load();
	nlibnss::load();
	nlibcurl::load();
	sysapp_load();
	nsyshid::load();
	nsyskbd::nsyskbd_load();
	swkbd::load();
	camera::load();
	procui_load();
}
