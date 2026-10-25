#pragma once

#include "ConfigValue.h"
#include "XMLConfig.h"

enum class NetworkService
{
	Offline,
	Nintendo,
	Pretendo,
	Samtendo,
	Custom,
	COUNT = Custom
};

struct NetworkConfig
{
    NetworkConfig()
	{

	};

	NetworkConfig(const NetworkConfig&) = delete;

    ConfigValue<std::string> networkname;
    ConfigValue<bool> disablesslver = false;
    struct
	{
		 ConfigValue<std::string> ACT;
		 ConfigValue<std::string> ECS;
		 ConfigValue<std::string> NUS;
		 ConfigValue<std::string> IAS;
		 ConfigValue<std::string> CCSU;
		 ConfigValue<std::string> CCS;
		 ConfigValue<std::string> IDBE;
		 ConfigValue<std::string> BOSS;
		 ConfigValue<std::string> TAGAYA;
		 ConfigValue<std::string> OLV;
	}urls{};

    public:
    static void LoadOnce();
    void Load(XMLConfigParser& parser);
    void Save(XMLConfigParser& parser);
    
    static bool XMLExists();
};

struct NintendoURLs {
   inline static std::string ACTURL = "https://account.nintendo.net";
   inline static std::string ECSURL = "https://ecs.wup.shop.nintendo.net/ecs/services/ECommerceSOAP";
   inline static std::string NUSURL = "https://nus.wup.shop.nintendo.net/nus/services/NetUpdateSOAP";
   inline static std::string IASURL = "https://ias.wup.shop.nintendo.net/ias/services/IdentityAuthenticationSOAP";
   inline static std::string CCSUURL = "https://ccs.wup.shop.nintendo.net/ccs/download";
   inline static std::string CCSURL = "http://ccs.cdn.wup.shop.nintendo.net/ccs/download";
   inline static std::string IDBEURL = "https://idbe-wup.cdn.nintendo.net/icondata";
   inline static std::string BOSSURL = "https://npts.app.nintendo.net/p01/tasksheet";
   inline static std::string TAGAYAURL = "https://tagaya.wup.shop.nintendo.net/tagaya/versionlist";
   inline static std::string OLVURL = "https://discovery.olv.nintendo.net/v1/endpoint";
};

struct PretendoURLs {
   inline static std::string ACTURL =  "https://account.pretendo.cc";
   inline static std::string ECSURL = "https://ecs.wup.shop.pretendo.cc/ecs/services/ECommerceSOAP";
   inline static std::string NUSURL = "https://nus.c.shop.pretendo.cc/nus/services/NetUpdateSOAP";
   inline static std::string IASURL = "https://ias.c.shop.pretendo.cc/ias/services/IdentityAuthenticationSOAP";
   inline static std::string CCSUURL = "https://ccs.c.shop.pretendo.cc/ccs/download";
   inline static std::string CCSURL = "http://ccs.cdn.c.shop.pretendo.cc/ccs/download";
   inline static std::string IDBEURL = "https://idbe-wup.cdn.pretendo.cc/icondata";
   inline static std::string BOSSURL = "https://npts.app.pretendo.cc/p01/tasksheet";
   inline static std::string TAGAYAURL = "https://tagaya.wup.shop.pretendo.cc/tagaya/versionlist";
   inline static std::string OLVURL = "https://discovery.olv.pretendo.cc/v1/endpoint";
};

struct SamtendoURLs {
   inline static std::string ACTURL =  "https://account.samtendo.net";
   inline static std::string ECSURL = "https://ecs.wup.shop.samtendo.net/ecs/services/ECommerceSOAP";
   inline static std::string NUSURL = "https://nus.c.shop.samtendo.net/nus/services/NetUpdateSOAP";
   inline static std::string IASURL = "https://ias.c.shop.samtendo.net/ias/services/IdentityAuthenticationSOAP";
   inline static std::string CCSUURL = "https://ccs.c.shop.samtendo.net/ccs/download";
   inline static std::string CCSURL = "http://ccs.cdn.c.shop.samtendo.net/ccs/download";
   inline static std::string IDBEURL = "https://idbe-wup.cdn.samtendo.net/icondata";
   inline static std::string BOSSURL = "https://npts.app.samtendo.net/p01/tasksheet";
   inline static std::string TAGAYAURL = "https://tagaya.wup.shop.samtendo.net/tagaya/versionlist";
   inline static std::string OLVURL = "https://api-olv.samtendo.net/v1/endpoint";
};


typedef XMLDataConfig<NetworkConfig> XMLNetworkConfig_t;
extern XMLNetworkConfig_t n_config;
inline NetworkConfig& GetNetworkConfig() { return n_config.data();};

inline bool IsNetworkServiceSSLDisabled(NetworkService service)
{
	if(service == NetworkService::Nintendo)
		return false;
	else if(service == NetworkService::Pretendo)
		return true;
	else if(service == NetworkService::Samtendo)
		return true;
	else if(service == NetworkService::Custom)
		return GetNetworkConfig().disablesslver.GetValue();
	return false;
}
