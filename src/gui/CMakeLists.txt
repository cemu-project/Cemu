project(CemuGui)

include_directories(".")

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

target_precompile_headers(CemuGui PRIVATE ../Common/precompiled.h)

target_include_directories(CemuGui PRIVATE ../)

target_link_libraries(CemuGui ${wxWidgets_LIBRARIES})
target_link_libraries(CemuGui libzip::zip)
target_link_libraries(CemuGui CemuCafe CemuComponents CemuResource)
target_link_libraries(CemuGui zarchive)

if(ENABLE_CUBEB)
    target_link_libraries(CemuGui cubeb)
endif()
