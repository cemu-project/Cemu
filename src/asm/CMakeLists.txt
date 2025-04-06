project(CemuAsm C)

if (CMAKE_OSX_ARCHITECTURES)
    set(CEMU_ASM_ARCHITECTURE ${CMAKE_OSX_ARCHITECTURES})
else()
    set(CEMU_ASM_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
endif()

if (CEMU_ASM_ARCHITECTURE MATCHES "(x86)|(X86)|(amd64)|(AMD64)")

	if (WIN32)

		enable_language(C ASM_MASM)

		add_library(CemuAsm x64util_masm.asm)
		set_source_files_properties(x64util_masm.asm PROPERTIES LANGUAGE ASM_MASM)

		# workaround for cr flag being passed to LINK.exe which considers it an input file and thus fails
		# doesn't always seem to happen. The Windows CI builds were fine, but locally I would run into this problem
		# possibly related to https://gitlab.kitware.com/cmake/cmake/-/issues/18889
		set(CMAKE_ASM_MASM_CREATE_STATIC_LIBRARY "<CMAKE_AR> /OUT:<TARGET> <LINK_FLAGS> <OBJECTS>")
	
		set_property(TARGET CemuAsm PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	else()

		# NASM
		if (APPLE)
			set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -g -Fdwarf -f macho64 --prefix _ -o <OBJECT> <SOURCE>")
		else()
			set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -g -Fdwarf -f elf64 -o <OBJECT> <SOURCE>")
		endif()
		set(CMAKE_ASM_NASM_LINK_EXECUTABLE "ld <FLAGS> <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> -fPIC <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")

		enable_language(C ASM_NASM)

		add_library(CemuAsm x64util_nasm.asm)
		set_source_files_properties(x64util_nasm.asm PROPERTIES LANGUAGE ASM_NASM)

		if (APPLE)
			set_target_properties(CemuAsm PROPERTIES NASM_OBJ_FORMAT macho64)
		else()
			set_target_properties(CemuAsm PROPERTIES NASM_OBJ_FORMAT elf64)
		endif()
		set_target_properties(CemuAsm PROPERTIES LINKER_LANGUAGE C)

	endif()

elseif(CEMU_ASM_ARCHITECTURE MATCHES "(aarch64)|(AARCH64)|(arm64)|(ARM64)")
	add_library(CemuAsm stub.cpp)
else()
	message(STATUS "CemuAsm - Unsupported arch: ${CEMU_ASM_ARCHITECTURE}")
endif()
