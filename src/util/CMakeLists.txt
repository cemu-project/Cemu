project(CemuUtil)

file(GLOB_RECURSE CPP_FILES *.cpp)
file(GLOB_RECURSE H_FILES *.h)

add_library(CemuUtil ${CPP_FILES} ${H_FILES})

target_include_directories(CemuUtil PUBLIC "../")

target_link_libraries(CemuUtil PRIVATE
	CemuCommon
	CemuConfig
	Boost::headers
	Boost::nowide
	OpenSSL::Crypto
)

if (ENABLE_WXWIDGETS)
	target_link_libraries(CemuUtil PRIVATE wx::base wx::core)
endif()
