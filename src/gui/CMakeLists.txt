project(CemuGui)

file(GLOB_RECURSE CPP_FILES *.cpp)
file(GLOB_RECURSE H_FILES *.h)
add_library(CemuGui ${CPP_FILES} ${H_FILES})

set_property(TARGET CemuGui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_sources(CemuGui PRIVATE
	wxcomponents/checkedlistctrl.cpp
	wxcomponents/checkedlistctrl.h
	wxcomponents/checktree.cpp
	wxcomponents/checktree.h
)

target_include_directories(CemuGui PUBLIC "../")
# PUBLIC because rapidjson/document.h is included in ChecksumTool.h
target_include_directories(CemuGui PUBLIC ${RAPIDJSON_INCLUDE_DIRS})

target_link_libraries(CemuGui PRIVATE
	CemuAudio
	CemuCafe
	CemuCommon
	CemuComponents
	CemuConfig
	CemuInput
	CemuResource
	CemuUtil
	Boost::headers
	CURL::libcurl
	libzip::zip
	OpenSSL::Crypto
	pugixml::pugixml
	ZArchive::zarchive
)

if(ENABLE_CUBEB)
	target_link_libraries(CemuGui PRIVATE cubeb::cubeb)
endif()

if (ENABLE_WXWIDGETS)
	# PUBLIC because wx/app.h is included in CemuApp.h
	target_link_libraries(CemuGui PUBLIC wx::base wx::core wx::gl wx::propgrid wx::xrc)
endif()
