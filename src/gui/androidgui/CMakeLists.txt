add_library(CemuAndroidGui
    AndroidWindowSystem.cpp
)

cemu_use_precompiled_header(CemuAndroidGui)

target_link_libraries(CemuAndroidGui PRIVATE
	CemuAudio
	CemuCafe
	CemuCommon
	CemuComponents
	CemuConfig
	CemuInput
	CemuResource
	CemuUtil
)