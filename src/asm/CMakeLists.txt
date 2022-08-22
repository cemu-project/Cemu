project(CemuAsm C)

IF (WIN32)

enable_language(C ASM_MASM)

add_library(CemuAsm
x64util_masm.asm
)
set_source_files_properties(x64util_masm.asm PROPERTIES LANGUAGE ASM_MASM)

ELSE()

# NASM
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -g -Fdwarf -f elf64 -o <OBJECT> <SOURCE>")
set(CMAKE_ASM_NASM_LINK_EXECUTABLE "ld <FLAGS> <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> -fPIC <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")

enable_language(C ASM_NASM)

add_library(CemuAsm
x64util_nasm.asm
)
set_source_files_properties(x64util_nasm.asm PROPERTIES LANGUAGE ASM_NASM)

set_target_properties(CemuAsm PROPERTIES NASM_OBJ_FORMAT elf64)
set_target_properties(CemuAsm PROPERTIES LINKER_LANGUAGE C)

ENDIF()

set_property(TARGET CemuAsm PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")