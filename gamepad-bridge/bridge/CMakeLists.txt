cmake_minimum_required(VERSION 3.20)
project(cemu-gamepad-bridge C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)

# Shared by the daemon and the test tools.
add_library(drcb_common STATIC
  src/canvas.cpp
  src/hid_parser.cpp
  src/hid_replay_file.cpp
  src/event_loop.cpp
  src/log.cpp
  src/shm_slots.cpp
  src/socket_io.cpp
  src/stats.cpp
  src/unix_server.cpp
)
target_include_directories(drcb_common PUBLIC include src)

add_executable(cemu-gamepad-bridge
  src/main.cpp
  src/cemu_link.cpp
  src/dsu_server.cpp
  src/uinput_pad.cpp
  src/replay_pad_link.cpp
  src/libdrc_pad_link.cpp
  src/drc_video_sender.cpp
  src/idle_source.cpp
  src/mock_pad_link.cpp
)
target_link_libraries(cemu-gamepad-bridge PRIVATE drcb_common)

add_executable(dsu-probe tools/dsu_probe.cpp)
target_link_libraries(dsu-probe PRIVATE drcb_common)

add_executable(fake-cemu tools/fake_cemu.cpp)
target_link_libraries(fake-cemu PRIVATE drcb_common)

find_package(PkgConfig REQUIRED)
find_package(SDL3 CONFIG)
if(SDL3_FOUND)
  pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec libavutil)
  add_executable(mock-pad tools/mock_pad.cpp src/drc_annexb.cpp)
  target_link_libraries(mock-pad PRIVATE drcb_common SDL3::SDL3 PkgConfig::AVCODEC)
else()
  message(WARNING "SDL3 not found: mock-pad will not be built")
endif()

# ---- pad video encoder: libdrc + patched x264, built by scripts/build-libdrc.sh ----
set(DRCB_BUILD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../build" CACHE PATH "where build-libdrc.sh put libdrc and x264")
find_package(PkgConfig REQUIRED)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale libavutil)
if(NOT EXISTS "${DRCB_BUILD_ROOT}/libdrc/libdrc.a" OR NOT EXISTS "${DRCB_BUILD_ROOT}/prefix/lib/libx264.a")
  message(FATAL_ERROR "libdrc/x264 not built: run scripts/build-libdrc.sh in the container first")
endif()
# DRCB_X264_ASM: link the 2013 x264 *with* its assembly (build/prefix-asm, configured without PIC).
# That code isn't position-independent, so every executable using the encoder is linked -no-pie.
# OFF uses the C-only x264 (build/prefix) that build-libdrc.sh makes. See docs/LIBDRC-BUILD.md.
option(DRCB_X264_ASM "link drc-x264 with assembly (requires -no-pie)" ON)
if(DRCB_X264_ASM)
  set(DRCB_X264_LIB "${DRCB_BUILD_ROOT}/prefix-asm/lib/libx264.a")
else()
  set(DRCB_X264_LIB "${DRCB_BUILD_ROOT}/prefix/lib/libx264.a")
endif()
if(NOT EXISTS "${DRCB_X264_LIB}")
  message(FATAL_ERROR "${DRCB_X264_LIB} not found")
endif()
add_library(drcb_encoder STATIC src/drc_encoder.cpp src/drc_annexb.cpp)
target_include_directories(drcb_encoder PUBLIC "${DRCB_BUILD_ROOT}/libdrc/include" "${DRCB_BUILD_ROOT}/prefix/include")
target_link_libraries(drcb_encoder PUBLIC drcb_common
  "${DRCB_BUILD_ROOT}/libdrc/libdrc.a" "${DRCB_X264_LIB}" PkgConfig::SWSCALE pthread m dl)
if(DRCB_X264_ASM)
  target_link_options(drcb_encoder INTERFACE -no-pie)
endif()

add_executable(tsf-probe tools/tsf_probe.cpp)
target_link_libraries(tsf-probe PRIVATE drcb_encoder)

add_executable(encode-bench tools/encode_bench.cpp)
target_link_libraries(encode-bench PRIVATE drcb_encoder)
add_executable(video-sender-test tools/video_sender_test.cpp src/drc_video_sender.cpp)
target_link_libraries(video-sender-test PRIVATE drcb_encoder pthread)
target_link_libraries(cemu-gamepad-bridge PRIVATE drcb_encoder)

# ---- tests ----
enable_testing()
# HID cross-check: Vanilla's real input packer (third_party, read-only) feeding our libdrc-transcribed parser.
set(VANILLA_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/vanilla/lib")
if(EXISTS "${VANILLA_LIB}/gamepad/input.c")
  add_executable(hid-crosscheck tests/hid_crosscheck.cpp tests/vanilla_stubs.c
    "${VANILLA_LIB}/gamepad/input.c" "${VANILLA_LIB}/util.c")
  target_include_directories(hid-crosscheck PRIVATE "${VANILLA_LIB}" "${VANILLA_LIB}/gamepad")
  set_source_files_properties("${VANILLA_LIB}/gamepad/input.c" "${VANILLA_LIB}/util.c" PROPERTIES COMPILE_OPTIONS "-w")
  target_link_libraries(hid-crosscheck PRIVATE drcb_common m pthread)
  add_test(NAME hid-crosscheck COMMAND hid-crosscheck)
  add_executable(hid-gen tests/hid_gen.cpp tests/vanilla_stubs.c "${VANILLA_LIB}/gamepad/input.c" "${VANILLA_LIB}/util.c")
  target_include_directories(hid-gen PRIVATE "${VANILLA_LIB}" "${VANILLA_LIB}/gamepad")
  target_link_libraries(hid-gen PRIVATE drcb_common m pthread)
else()
  message(WARNING "third_party/vanilla missing: hid-crosscheck not built")
endif()
