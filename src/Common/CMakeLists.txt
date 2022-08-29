add_library(Common
  betype.h
  enumFlags.h
  ExceptionHandler/ExceptionHandler.cpp
  ExceptionHandler/ExceptionHandler.h
  filestream.h
  GLInclude/glext.h
  GLInclude/glFunctions.h
  GLInclude/GLInclude.h
  GLInclude/glxext.h
  GLInclude/khrplatform.h
  GLInclude/wglext.h
  unix/date.h
  unix/fast_float.h
  MemPtr.h
  platform.h
  precompiled.cpp
  precompiled.h
  socket.h
  StackAllocator.h
  SysAllocator.cpp
  SysAllocator.h
  version.h
  zstring_view.h
)

target_precompile_headers(Common PUBLIC precompiled.h)

if (WIN32)
  target_sources(Common PRIVATE windows/platform.cpp windows/platform.h)
else()
  target_sources(Common PRIVATE unix/platform.cpp unix/platform.h)
endif()


target_link_libraries(Common PRIVATE Cafe Config Components)

target_link_libraries(Common PRIVATE
  Boost::nowide
  Boost::filesystem
  glm::glm
)

# PUBLIC because:
# - boost/predef/os.h is included in platform.h
# - fmt/core.h is included in precompiled.h
target_link_libraries(Common PUBLIC Boost::headers fmt::fmt)

if (UNIX)
  target_link_libraries(Common PRIVATE X11::X11 X11::Xrender X11::Xutil)
endif()
