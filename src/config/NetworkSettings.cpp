#include "NetworkSettings.h"
#include "LaunchSettings.h"
#include "CemuConfig.h"
#include <boost/dll/runtime_symbol_info.hpp>
#include "Common/FileStream.h"
XMLNetworkConfig_t n_config(L"network_services.xml");

void NetworkConfig::LoadOnce() {
    s_full_path = boost::dll::program_location().generic_wstring();
	s_path = s_full_path.parent_path();
   n_config.SetFilename(GetPath("network_services.xml").generic_wstring());
   if(XMLExists()) {
   n_config.Load();
   }
}

void NetworkConfig::Load(XMLConfigParser& parser){
    auto config = parser.get("content");
    networkname = config.get("networkname","[Nintendo]");
    disablesslver = config.get("disablesslverification",disablesslver);
    auto u = config.get("urls");
    urls.ACT = u.get("act",NintendoURLs::ACTURL);
    urls.ECS = u.get("ecs",NintendoURLs::ECSURL);
    urls.NUS = u.get("nus",NintendoURLs::NUSURL);
    urls.IAS = u.get("ias",NintendoURLs::IASURL);
    urls.CCSU = u.get("ccsu",NintendoURLs::CCSUURL);
    urls.CCS = u.get("ccs",NintendoURLs::CCSURL);
    urls.IDBE = u.get("idbe",NintendoURLs::IDBEURL);
    urls.BOSS = u.get("boss",NintendoURLs::BOSSURL);
    urls.TAGAYA = u.get("tagaya",NintendoURLs::TAGAYAURL);
    if (static_cast<NetworkService>(GetConfig().account.active_service.GetValue()) == NetworkService::Custom) {
        LaunchSettings::ChangeNetworkServiceURL(2);
    }
}
bool NetworkConfig::XMLExists() {
    if (!fs::exists(GetPath("network_services.xml"))) {
         if (static_cast<NetworkService>(GetConfig().account.active_service.GetValue()) == NetworkService::Custom) {
         LaunchSettings::ChangeNetworkServiceURL(0);
         GetConfig().account.active_service = 0;
         }
   return false;
    } else {return true;}
}