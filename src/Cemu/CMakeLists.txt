add_library(Components
  ExpressionParser/ExpressionParser.cpp
  ExpressionParser/ExpressionParser.h
  FileCache/FileCache.cpp
  FileCache/FileCache.h
  Logging/CemuDebugLogging.h
  Logging/CemuLogging.cpp
  Logging/CemuLogging.h
  napi/napi.h
  napi/napi_act.cpp
  napi/napi_ec.cpp
  napi/napi_helper.cpp
  napi/napi_helper.h
  napi/napi_idbe.cpp
  napi/napi_version.cpp
  ncrypto/ncrypto.cpp
  ncrypto/ncrypto.h
  nex/nex.cpp
  nex/nex.h
  nex/nexFriends.cpp
  nex/nexFriends.h
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

target_precompile_headers(Components PRIVATE ../Common/precompiled.h)

target_link_libraries(Components PRIVATE
  Cafe
  Common
  Config
  Gui
  Util
)

target_link_libraries(Components PRIVATE
  Boost::headers
  CURL::libcurl
  OpenSSL::Crypto
  OpenSSL::SSL
  pugixml::pugixml
  ZLIB::ZLIB
)

# PUBLIC because fmt/format.h is included in ExpressionParser/ExpressionParser.h
target_link_libraries(Components PUBLIC fmt::fmt)

if (ENABLE_DISCORD_RPC)
  target_sources(Components PRIVATE
    DiscordPresence/DiscordPresence.cpp
    DiscordPresence/DiscordPresence.h
  )
  target_link_libraries(Components PRIVATE discord-rpc)
endif()
