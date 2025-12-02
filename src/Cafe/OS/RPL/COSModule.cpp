#include "COSModule.h"

#include "Cafe/OS/libs/coreinit/coreinit.h"
#include "Cafe/OS/libs/zlib125/zlib125.h"
#include "OS/libs/gx2/GX2.h"
#include "OS/libs/dmae/dmae.h"
#include "OS/libs/padscore/padscore.h"
#include "OS/libs/vpad/vpad.h"
#include "OS/libs/snd_core/ax.h"
#include "OS/libs/snd_user/snd_user.h"
#include "OS/libs/mic/mic.h"
#include "OS/libs/erreula/erreula.h"
#include "OS/libs/nlibnss/nlibnss.h"
#include "OS/libs/nn_acp/nn_acp.h"
#include "OS/libs/nn_act/nn_act.h"
#include "OS/libs/nn_acp/nn_acp.h"
#include "OS/libs/nn_ac/nn_ac.h"
#include "OS/libs/nn_boss/nn_boss.h"
#include "OS/libs/nn_ec/nn_ec.h"
#include "OS/libs/nn_boss/nn_boss.h"
#include "OS/libs/nn_nfp/nn_nfp.h"
#include "OS/libs/nn_uds/nn_uds.h"
#include "OS/libs/nn_nim/nn_nim.h"
#include "OS/libs/nn_ndm/nn_ndm.h"
#include "OS/libs/nn_spm/nn_spm.h"
#include "OS/libs/nn_save/nn_save.h"
#include "OS/libs/nsysnet/nsysnet.h"
#include "OS/libs/nn_fp/nn_fp.h"
#include "OS/libs/nn_idbe/nn_idbe.h"
#include "OS/libs/nn_olv/nn_olv.h"
#include "OS/libs/nn_idbe/nn_idbe.h"
#include "OS/libs/nlibnss/nlibnss.h"
#include "OS/libs/nlibcurl/nlibcurl.h"
#include "OS/libs/sysapp/sysapp.h"
#include "OS/libs/nsyshid/nsyshid.h"
#include "OS/libs/nsyskbd/nsyskbd.h"
#include "OS/libs/swkbd/swkbd.h"
#include "OS/libs/camera/camera.h"
#include "OS/libs/proc_ui/proc_ui.h"
#include "OS/libs/avm/avm.h"
#include "OS/libs/drmapp/drmapp.h"
#include "OS/libs/TCL/TCL.h"
#include "OS/libs/nn_cmpt/nn_cmpt.h"
#include "OS/libs/nn_ccr/nn_ccr.h"
#include "OS/libs/nn_temp/nn_temp.h"
#include "OS/libs/nn_aoc/nn_aoc.h"
#include "OS/libs/nn_pdm/nn_pdm.h"
#include "OS/libs/h264_avc/h264dec.h"
#include "OS/libs/ntag/ntag.h"
#include "OS/libs/nfc/nfc.h"

std::span<COSModule*> GetCOSModules()
{
	static std::vector<COSModule*> s_cosModules =
	{
		coreinit::GetModule(),
		zlib::GetModule(),
		GX2::GetModule(),
		dmae::GetModule(),
		padscore::GetModule(),
		vpad::GetModule(),
		snd_core::GetModuleSndCore1(),
		snd_core::GetModuleSndCore2(),
		snd_user::GetModuleSndUser1(),
		snd_user::GetModuleSndUser2(),
		mic::GetModule(),
		nn::erreula::GetModule(),
		nn::act::GetModule(),
		nn::acp::GetModule(),
		nn::ac::GetModule(),
		nn::ec::GetModule(),
		nn::boss::GetModule(),
		nn::nfp::GetModule(),
		nn::uds::GetModule(),
		nn::nim::GetModule(),
		nn::ndm::GetModule(),
		nn::spm::GetModule(),
		nn::save::GetModule(),
		nsysnet::GetModule(),
		nn::fp::GetModule(),
		nn::olv::GetModule(),
		nn::idbe::GetModule(),
		nlibnss::GetModule(),
		nlibcurl::GetModule(),
		sysapp::GetModule(),
		nsyshid::GetModule(),
		nsyskbd::GetModule(),
		swkbd::GetModule(),
		camera::GetModule(),
		proc_ui::GetModule(),
		avm::GetModule(),
		drmapp::GetModule(),
		TCL::GetModule(),
		nn::cmpt::GetModule(),
		nn::ccr::GetModule(),
		nn::temp::GetModule(),
		nn::aoc::GetModule(),
		nn::pdm::GetModule(),
		H264::GetModule(),
		ntag::GetModule(),
		nfc::GetModule(),
	};
	return s_cosModules;
}
