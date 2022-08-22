project(CemuAudio)

add_library(CemuAudio
IAudioAPI.cpp
IAudioAPI.h
)

set_property(TARGET CemuAudio PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# move these to UI folder
target_sources(CemuAudio PRIVATE
  audioDebuggerWindow.cpp
  audioDebuggerWindow.h
)

if(WIN32)
  target_sources(CemuAudio PRIVATE
      DirectSoundAPI.cpp
      DirectSoundAPI.h
      XAudio2API.cpp
      XAudio2API.h
      XAudio27API.cpp
      XAudio27API.h
  )
endif()

if(ENABLE_CUBEB)
  target_sources(CemuAudio PRIVATE
          CubebAPI.cpp
          CubebAPI.h
  )
  #add_definitions(HAS_CUBEB)
endif()

target_precompile_headers(CemuAudio PRIVATE ../Common/precompiled.h)

#target_link_libraries(CemuAudio CemuCommon CemuGui CemuPlatform)
target_include_directories(CemuAudio PRIVATE ../)

if(ENABLE_CUBEB)
  target_link_libraries(CemuAudio cubeb)
endif()
