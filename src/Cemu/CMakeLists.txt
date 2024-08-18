add_library(CemuComponents
  DiscordPresence/DiscordPresence.cpp
  DiscordPresence/DiscordPresence.h
  ExpressionParser/ExpressionParser.cpp
  ExpressionParser/ExpressionParser.h
  FileCache/FileCache.cpp
  FileCache/FileCache.h
  Logging/CemuDebugLogging.h
  Logging/CemuLogging.cpp
  Logging/CemuLogging.h
  napi/napi_act.cpp
  napi/napi_ec.cpp
  napi/napi.h
  napi/napi_helper.cpp
  napi/napi_helper.h
  napi/napi_idbe.cpp
  napi/napi_version.cpp
  ncrypto/ncrypto.cpp
  ncrypto/ncrypto.h
  nex/nex.cpp
  nex/nexFriends.cpp
  nex/nexFriends.h
  nex/nex.h
  nex/nexThread.cpp
  nex/nexThread.h
  nex/nexTypes.h
  nex/prudp.cpp
  nex/prudp.h
  PPCAssembler/ppcAssembler.cpp
  PPCAssembler/ppcAssembler.h
  Tools/DownloadManager/DownloadManager.cpp
  Tools/DownloadManager/DownloadManager.h
)

set_property(TARGET CemuComponents PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(CemuComponents PUBLIC "../")

target_link_libraries(CemuComponents PRIVATE
	CemuCafe
	CemuCommon
	CemuConfig
	CemuGui
	CemuUtil
	Boost::headers
	CURL::libcurl
	OpenSSL::Crypto
	OpenSSL::SSL
	pugixml::pugixml
	ZLIB::ZLIB
)

# PUBLIC because fmt/format.h is included in ExpressionParser/ExpressionParser.h
target_link_libraries(CemuComponents PUBLIC fmt::fmt)

if(ENABLE_DISCORD_RPC)
	target_link_libraries(CemuComponents PRIVATE discord-rpc)
endif()
