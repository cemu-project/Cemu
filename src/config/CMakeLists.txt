project(CemuConfig)

file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
add_library(CemuConfig ${CPP_FILES} ${H_FILES})

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
