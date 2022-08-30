project(CemuCommon)

file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
add_library(CemuCommon ${CPP_FILES} ${H_FILES})

set_property(TARGET CemuCommon PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(WIN32)
target_sources(CemuCommon
PRIVATE
    windows/platform.cpp
    windows/platform.h
)
else()
target_sources(CemuCommon
PRIVATE
    unix/platform.cpp
    unix/platform.h
)
endif()

target_sources(CemuCommon
        PRIVATE
        ExceptionHandler/ExceptionHandler.cpp
        ExceptionHandler/ExceptionHandler.h
        )

target_precompile_headers(CemuCommon PUBLIC precompiled.h)
target_include_directories(CemuCommon PUBLIC "../")

target_link_libraries(CemuCommon PRIVATE
	CemuCafe
	CemuConfig
	CemuComponents
	Boost::nowide
	Boost::filesystem
	glm::glm
)

if (UNIX AND NOT APPLE)
	target_link_libraries(CemuCommon PRIVATE X11::X11 X11::Xrender X11::Xutil)
endif()

# PUBLIC because:
# - boost/predef/os.h is included in platform.h
# - fmt/core.h is included in precompiled.h
target_link_libraries(CemuCommon PUBLIC Boost::headers fmt::fmt)
