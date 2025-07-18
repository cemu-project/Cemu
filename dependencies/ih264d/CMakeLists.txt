﻿cmake_minimum_required (VERSION 3.8)

project ("ih264d")

add_library (ih264d 
"common/ih264_buf_mgr.c"
"common/ih264_buf_mgr.h"
"common/ih264_cabac_tables.c"
"common/ih264_cabac_tables.h"
"common/ih264_cavlc_tables.c"
"common/ih264_cavlc_tables.h"
"common/ih264_chroma_intra_pred_filters.c"
"common/ih264_common_tables.c"
"common/ih264_common_tables.h"
"common/ih264_deblk_edge_filters.c"
"common/ih264_deblk_edge_filters.h"
"common/ih264_deblk_tables.c"
"common/ih264_deblk_tables.h"
"common/ih264_debug.h"
"common/ih264_defs.h"
"common/ih264_disp_mgr.c"
"common/ih264_disp_mgr.h"
"common/ih264_dpb_mgr.c"
"common/ih264_dpb_mgr.h"
"common/ih264_error.h"
"common/ih264_ihadamard_scaling.c"
"common/ih264_inter_pred_filters.c"
"common/ih264_inter_pred_filters.h"
"common/ih264_intra_pred_filters.h"
"common/ih264_iquant_itrans_recon.c"
"common/ih264_list.c"
"common/ih264_list.h"
"common/ih264_luma_intra_pred_filters.c"
"common/ih264_macros.h"
"common/ih264_mem_fns.c"
"common/ih264_mem_fns.h"
"common/ih264_padding.c"
"common/ih264_padding.h"
"common/ih264_resi_trans.h"
"common/ih264_resi_trans_quant.c"
"common/ih264_size_defs.h"
"common/ih264_structs.h"
"common/ih264_trans_data.c"
"common/ih264_trans_data.h"
"common/ih264_trans_macros.h"
"common/ih264_trans_quant_itrans_iquant.h"
"common/ih264_typedefs.h"
"common/ih264_weighted_pred.c"
"common/ih264_weighted_pred.h"
"common/ithread.c"
"common/ithread.h"
"decoder/ih264d.h"
"decoder/ih264d_api.c"
"decoder/ih264d_bitstrm.c"
"decoder/ih264d_bitstrm.h"
"decoder/ih264d_cabac.c"
"decoder/ih264d_cabac.h"
"decoder/ih264d_cabac_init_tables.c"
"decoder/ih264d_compute_bs.c"
"decoder/ih264d_deblocking.c"
"decoder/ih264d_deblocking.h"
"decoder/ih264d_debug.h"
"decoder/ih264d_defs.h"
"decoder/ih264d_dpb_manager.h"
"decoder/ih264d_dpb_mgr.c"
"decoder/ih264d_error_handler.h"
"decoder/ih264d_format_conv.c"
"decoder/ih264d_format_conv.h"
"decoder/ih264d_function_selector.h"
"decoder/ih264d_function_selector_generic.c"
"decoder/ih264d_inter_pred.c"
"decoder/ih264d_inter_pred.h"
"decoder/ih264d_mb_utils.c"
"decoder/ih264d_mb_utils.h"
"decoder/ih264d_mem_request.h"
"decoder/ih264d_mvpred.c"
"decoder/ih264d_mvpred.h"
"decoder/ih264d_nal.c"
"decoder/ih264d_nal.h"
"decoder/ih264d_parse_bslice.c"
"decoder/ih264d_parse_cabac.c"
"decoder/ih264d_parse_cabac.h"
"decoder/ih264d_parse_cavlc.c"
"decoder/ih264d_parse_cavlc.h"
"decoder/ih264d_parse_headers.c"
"decoder/ih264d_parse_headers.h"
"decoder/ih264d_parse_islice.c"
"decoder/ih264d_parse_islice.h"
"decoder/ih264d_parse_mb_header.c"
"decoder/ih264d_parse_mb_header.h"
"decoder/ih264d_parse_pslice.c"
"decoder/ih264d_parse_slice.c"
"decoder/ih264d_parse_slice.h"
"decoder/ih264d_process_bslice.c"
"decoder/ih264d_process_bslice.h"
"decoder/ih264d_process_intra_mb.c"
"decoder/ih264d_process_intra_mb.h"
"decoder/ih264d_process_pslice.c"
"decoder/ih264d_process_pslice.h"
"decoder/ih264d_quant_scaling.c"
"decoder/ih264d_quant_scaling.h"
"decoder/ih264d_sei.c"
"decoder/ih264d_sei.h"
"decoder/ih264d_structs.h"
"decoder/ih264d_tables.c"
"decoder/ih264d_tables.h"
"decoder/ih264d_thread_compute_bs.c"
"decoder/ih264d_thread_compute_bs.h"
"decoder/ih264d_thread_parse_decode.c"
"decoder/ih264d_thread_parse_decode.h"
"decoder/ih264d_transfer_address.h"
"decoder/ih264d_utils.c"
"decoder/ih264d_utils.h"
"decoder/ih264d_vui.c"
"decoder/ih264d_vui.h"
"decoder/iv.h"
"decoder/ivd.h"
)

if (CMAKE_OSX_ARCHITECTURES)
set(IH264D_ARCHITECTURE ${CMAKE_OSX_ARCHITECTURES})
else()
set(IH264D_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
endif()

if (IH264D_ARCHITECTURE STREQUAL "x86_64" OR IH264D_ARCHITECTURE STREQUAL "amd64" OR IH264D_ARCHITECTURE STREQUAL "AMD64")
set(LIBAVCDEC_X86_INCLUDES "common/x86" "decoder/x86")
include_directories("common/" "decoder/" ${LIBAVCDEC_X86_INCLUDES})
target_sources(ih264d PRIVATE
"common/x86/ih264_chroma_intra_pred_filters_ssse3.c"
"common/x86/ih264_deblk_chroma_ssse3.c"
"common/x86/ih264_deblk_luma_ssse3.c"
"common/x86/ih264_ihadamard_scaling_sse42.c"
"common/x86/ih264_ihadamard_scaling_ssse3.c"
"common/x86/ih264_inter_pred_filters_ssse3.c"
"common/x86/ih264_iquant_itrans_recon_dc_ssse3.c"
"common/x86/ih264_iquant_itrans_recon_sse42.c"
"common/x86/ih264_iquant_itrans_recon_ssse3.c"
"common/x86/ih264_luma_intra_pred_filters_ssse3.c"
"common/x86/ih264_mem_fns_ssse3.c"
"common/x86/ih264_padding_ssse3.c"
"common/x86/ih264_platform_macros.h"
"common/x86/ih264_resi_trans_quant_sse42.c"
"common/x86/ih264_weighted_pred_sse42.c"
"decoder/x86/ih264d_function_selector.c"
"decoder/x86/ih264d_function_selector_sse42.c"
"decoder/x86/ih264d_function_selector_ssse3.c"
)
elseif(IH264D_ARCHITECTURE STREQUAL "aarch64" OR IH264D_ARCHITECTURE STREQUAL "arm64")
enable_language( C CXX ASM )
set(LIBAVCDEC_ARM_INCLUDES "common/armv8" "decoder/arm")
include_directories("common/" "decoder/" ${LIBAVCDEC_ARM_INCLUDES})
target_sources(ih264d PRIVATE
"common/armv8/ih264_deblk_chroma_av8.s"
"common/armv8/ih264_deblk_luma_av8.s"
"common/armv8/ih264_default_weighted_pred_av8.s"
"common/armv8/ih264_ihadamard_scaling_av8.s"
"common/armv8/ih264_inter_pred_chroma_av8.s"
"common/armv8/ih264_inter_pred_filters_luma_horz_av8.s"
"common/armv8/ih264_inter_pred_filters_luma_vert_av8.s"
"common/armv8/ih264_inter_pred_luma_copy_av8.s"
"common/armv8/ih264_inter_pred_luma_horz_hpel_vert_hpel_av8.s"
"common/armv8/ih264_inter_pred_luma_horz_hpel_vert_qpel_av8.s"
"common/armv8/ih264_inter_pred_luma_horz_qpel_av8.s"
"common/armv8/ih264_inter_pred_luma_horz_qpel_vert_hpel_av8.s"
"common/armv8/ih264_inter_pred_luma_horz_qpel_vert_qpel_av8.s"
"common/armv8/ih264_inter_pred_luma_vert_qpel_av8.s"
"common/armv8/ih264_intra_pred_chroma_av8.s"
"common/armv8/ih264_intra_pred_luma_16x16_av8.s"
"common/armv8/ih264_intra_pred_luma_4x4_av8.s"
"common/armv8/ih264_intra_pred_luma_8x8_av8.s"
"common/armv8/ih264_iquant_itrans_recon_av8.s"
"common/armv8/ih264_iquant_itrans_recon_dc_av8.s"
"common/armv8/ih264_mem_fns_neon_av8.s"
"common/armv8/ih264_neon_macros.s"
"common/armv8/ih264_padding_neon_av8.s"
"common/armv8/ih264_platform_macros.h"
"common/armv8/ih264_resi_trans_quant_av8.s"
"common/armv8/ih264_weighted_bi_pred_av8.s"
"common/armv8/ih264_weighted_pred_av8.s"
"decoder/arm/ih264d_function_selector_a9q.c"
"decoder/arm/ih264d_function_selector_av8.c"
"decoder/arm/ih264d_function_selector.c"
)
target_compile_options(ih264d PRIVATE -DARMV8 $<$<COMPILE_LANGUAGE:ASM,Clang>:-Wno-unused-command-line-argument>)
if(NOT MSVC)
    set(CMAKE_ASM_FLAGS "${CFLAGS} -x assembler-with-cpp")
endif()
if(APPLE)
    target_sources(ih264d PRIVATE "common/armv8/macos_arm_symbol_aliases.s")
endif()
else()
message(FATAL_ERROR "ih264d unknown architecture: ${IH264D_ARCHITECTURE}")
endif()

if(MSVC)
set_property(TARGET ih264d PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# tune settings for slightly better performance
target_compile_options(ih264d PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/Oi>) # enable intrinsic functions
target_compile_options(ih264d PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/Ot>) # favor speed
target_compile_options(ih264d PRIVATE "/GS-") # disable runtime checks

endif()
