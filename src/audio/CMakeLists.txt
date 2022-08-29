add_library(Audio IAudioAPI.cpp IAudioAPI.h)

target_precompile_headers(Audio PRIVATE ../Common/precompiled.h)

# move these to UI folder
target_sources(Audio PRIVATE audioDebuggerWindow.cpp audioDebuggerWindow.h)

if (WIN32)
  target_sources(Audio PRIVATE
    DirectSoundAPI.cpp
    DirectSoundAPI.h
    XAudio2API.cpp
    XAudio2API.h
    XAudio27API.cpp
    XAudio27API.h
  )
endif()

target_link_libraries(Audio PRIVATE Cafe Config Gui Util)

if (ENABLE_CUBEB)
  target_sources(Audio PRIVATE CubebAPI.cpp CubebAPI.h)
  # PUBLIC because cubeb.h/cubeb.h is included in CubebAPI.h
  target_link_libraries(Audio PUBLIC cubeb::cubeb)
endif()

if (ENABLE_WXWIDGETS)
	# PUBLIC because wx/wx.h is included in audioDebuggerWindow.h
	target_link_libraries(Audio PUBLIC wx::base wx::core)
endif()
