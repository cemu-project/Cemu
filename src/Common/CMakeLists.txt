add_library(CemuCommon 
  betype.h
  cpu_features.cpp
  cpu_features.h
  enumFlags.h
  ExceptionHandler/ExceptionHandler.cpp
  ExceptionHandler/ExceptionHandler.h
  FileStream.h
  GLInclude/glext.h
  GLInclude/glFunctions.h
  GLInclude/GLInclude.h
  GLInclude/glxext.h
  GLInclude/khrplatform.h
  GLInclude/wglext.h
  MemPtr.h
  platform.h
  precompiled.cpp
  precompiled.h
  socket.h
  StackAllocator.h
  SysAllocator.cpp
  SysAllocator.h
  CafeString.h
  version.h
)

set_property(TARGET CemuCommon PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(WIN32)
	target_sources(CemuCommon PRIVATE
		windows/platform.cpp
		windows/platform.h
		windows/FileStream_win32.cpp
		ExceptionHandler/ExceptionHandler_win32.cpp
	)
else()
target_sources(CemuCommon
PRIVATE
    unix/date.h
    unix/fast_float.h
    unix/platform.cpp
    unix/platform.h
    unix/FileStream_unix.cpp
    unix/FileStream_unix.h
    ExceptionHandler/ExceptionHandler_posix.cpp
)
endif()

if(UNIX AND NOT APPLE)
	target_sources(CemuCommon PRIVATE
		ExceptionHandler/ELFSymbolTable.cpp
		ExceptionHandler/ELFSymbolTable.h
	)
endif()

# All the targets wanting to use the precompiled.h header
# have to link to CemuCommon
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
