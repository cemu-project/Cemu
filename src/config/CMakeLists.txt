add_library(CemuConfig
    ActiveSettings.cpp
    ActiveSettings.h
    CemuConfig.cpp
    CemuConfig.h
    ConfigValue.h
    LaunchSettings.cpp
    LaunchSettings.h
    NetworkSettings.cpp
    NetworkSettings.h
    XMLConfig.h
)

set_property(TARGET CemuConfig PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(CemuConfig PUBLIC "../")

target_link_libraries(CemuConfig PRIVATE
	CemuCafe
	CemuCommon
	CemuUtil
	Boost::headers
	Boost::program_options
	pugixml::pugixml
)

if (ENABLE_WXWIDGETS)
	# PUBLIC because wx/language.h is included in CemuConfig.h
	# Could be made PRIVATE by using 0 instead of wxLANGUAGE_DEFAULT
	target_link_libraries(CemuConfig PUBLIC wx::base wx::core)
endif()
