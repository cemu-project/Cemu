add_library(Resource CafeDefaultFont.cpp)

if (UNIX)
  target_sources(Resource PRIVATE embedded/resources.cpp embedded/resources.h)
endif()

target_precompile_headers(Resource PRIVATE ../Common/precompiled.h)
