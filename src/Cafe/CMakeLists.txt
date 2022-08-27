project(CemuCafe)

include_directories(".")

if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
	add_compile_options(-mssse3 -mavx2)
endif()

file(GLOB_RECURSE CPP_FILES *.cpp)
file(GLOB_RECURSE H_FILES *.h)
add_library(CemuCafe ${CPP_FILES} ${H_FILES})


set_property(TARGET CemuCafe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_precompile_headers(CemuCafe PRIVATE ../Common/precompiled.h)

target_include_directories(CemuCafe PRIVATE ../)

#target_link_libraries(CemuCafe ZArchivexx)
#target_link_libraries(CemuCafe CemuCommon CemuCore CemuConfig CemuUtil CemuResource)
#target_link_libraries(CemuCafe OpenSSL::SSL)
#target_link_libraries(CemuCafe ZLIB::ZLIB)
#target_link_libraries(CemuCafe imgui::imgui)
#target_link_libraries(CemuCafe imguiImpl)
#target_link_libraries(CemuCafe pugixml pugixml::static pugixml::pugixml)
#target_link_libraries(CemuCafe libzip::zip)
target_link_libraries(CemuCafe glslang SPIRV)
target_link_libraries(CemuCafe ih264d zarchive)
#target_link_libraries(CemuCafe zstd::libzstd_static)

IF(WIN32)
target_link_libraries(CemuCafe iphlpapi)
ENDIF()
