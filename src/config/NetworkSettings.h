#pragma once

#include "ConfigValue.h"
#include "XMLConfig.h"


enum NetworkService {
Nintendo = 0,
Pretendo = 1,
Custom = 2
};
struct NetworkConfig {
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
	}urls{};

    public:
    static void LoadOnce();
    void Load(XMLConfigParser& parser);
    void Save(XMLConfigParser& parser);
    
    inline static bool XMLExists();
    private:
    inline static fs::path s_path;
    inline static fs::path s_full_path;
    [[nodiscard]] static fs::path GetPath(std::string_view p) 
	{
		std::basic_string_view<char8_t> s((const char8_t*)p.data(), p.size());
		return s_path / fs::path(s);
	}
};

struct NintendoURLs {
   inline static std::string NUSURL = "https://nus.wup.shop.nintendo.net/nus/services/NetUpdateSOAP";
   inline static std::string IASURL = "https://ias.wup.shop.nintendo.net/ias/services/IdentityAuthenticationSOAP";
   inline static std::string CCSUURL = "https://ccs.wup.shop.nintendo.net/ccs/download";
   inline static std::string CCSURL = "http://ccs.cdn.wup.shop.nintendo.net/ccs/download";
   inline static std::string IDBEURL = "https://idbe-wup.cdn.nintendo.net/icondata";
   inline static std::string BOSSURL = "https://npts.app.nintendo.net/p01/tasksheet";
};

struct PretendoURLs {
   inline static std::string NUSURL = "https://nus.c.shop.pretendo.cc/nus/services/NetUpdateSOAP";
   inline static std::string IASURL = "https://ias.c.shop.pretendo.cc/ias/services/IdentityAuthenticationSOAP";
   inline static std::string CCSUURL = "https://ccs.c.shop.pretendo.cc/ccs/download";
   inline static std::string CCSURL = "http://ccs.cdn.c.shop.pretendo.cc/ccs/download";
   inline static std::string IDBEURL = "https://idbe-wup.cdn.pretendo.cc/icondata";
   inline static std::string BOSSURL = "https://npts.app.pretendo.cc/p01/tasksheet";
};

typedef XMLDataConfig<NetworkConfig, &NetworkConfig::Load, &NetworkConfig::Save> XMLNetworkConfig_t;
extern XMLNetworkConfig_t n_config;
inline NetworkConfig& GetNetworkConfig() { return n_config.data();};