if (WIN32)
  enable_language(C ASM_MASM)

  add_library(Assembly x64util_masm.asm)
  set_source_files_properties(x64util_masm.asm PROPERTIES LANGUAGE ASM_MASM)

else()
  enable_language(C ASM_NASM)

  add_library(Assembly x64util_nasm.asm)
  set_target_properties(Assembly PROPERTIES LINKER_LANGUAGE C)
  set_source_files_properties(x64util_nasm.asm PROPERTIES LANGUAGE ASM_NASM)

  set(CMAKE_ASM_NASM_LINK_EXECUTABLE "ld <FLAGS> <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> -fPIC <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")

  if (APPLE)
    set_target_properties(Assembly PROPERTIES NASM_OBJ_FORMAT macho64)
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -g -Fdwarf -f macho64 --prefix _ -o <OBJECT> <SOURCE>")
  else()
    set_target_properties(Assembly PROPERTIES NASM_OBJ_FORMAT elf64)
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -g -Fdwarf -f elf64 -o <OBJECT> <SOURCE>")
  endif()
endif()
