add_library(CemuGui
	guiWrapper.h
)

if(ENABLE_WXWIDGETS)
	add_subdirectory(wxGui)
	target_link_libraries(CemuGui PRIVATE CemuwxGui)
endif()
