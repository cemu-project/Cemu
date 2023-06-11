add_library(CemuGui
	guiWrapper.h
)

if(ANDROID)
	add_subdirectory(AndroidGui)
	target_link_libraries(CemuGui PRIVATE CemuAndroidGui)
endif()

if(ENABLE_WXWIDGETS)
	add_subdirectory(wxGui)
	target_link_libraries(CemuGui PRIVATE CemuwxGui)
endif()
