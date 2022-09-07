add_library(CemuResource)

# icon resources
if(UNIX)
	target_sources(CemuResource PRIVATE
	embedded/resources.cpp
	embedded/resources.h
	)
endif()

target_sources(CemuResource PRIVATE CafeDefaultFont.cpp)

target_include_directories(CemuResource PUBLIC "../")

target_link_libraries(CemuResource PRIVATE CemuCommon CemuComponents)
