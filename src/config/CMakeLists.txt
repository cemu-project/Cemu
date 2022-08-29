add_library(Config 
  ActiveSettings.cpp
  ActiveSettings.h
  CemuConfig.cpp
  CemuConfig.h
  ConfigValue.h
  LaunchSettings.cpp
  LaunchSettings.h
  PermanentConfig.cpp
  PermanentConfig.h
  PermanentStorage.cpp
  PermanentStorage.cpp
  XMLConfig.h
)

target_precompile_headers(Config PRIVATE ../Common/precompiled.h)

target_link_libraries(Config PRIVATE Cafe Common Util)

target_link_libraries(Config PRIVATE Boost::headers Boost::program_options pugixml::pugixml)

if (ENABLE_WXWIDGETS)
  # PUBLIC because wx/language.h is included in CemuConfig.h
  # Could be made PRIVATE by using 0 instead of wxLANGUAGE_DEFAULT
  target_link_libraries(Config PUBLIC wx::base wx::core)
endif()
