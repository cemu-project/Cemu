cmake_minimum_required(VERSION 3.10)

project(ih264d LANGUAGES C)

add_library(ih264d STATIC
    "common/ih264_buf_mgr.c"
    "common/ih264_cabac_tables.c"
    "common/ih264_cavlc_tables.c"
    "common/ih264_chroma_intra_pred_filters.c"
    "common/ih264_common_tables.c"
    "common/ih264_deblk_edge_filters.c"
    "common/ih264_deblk_tables.c"
    "common/ih264_disp_mgr.c"
    "common/ih264_dpb_mgr.c"
    "common/ih264_ihadamard_scaling.c"
    "common/ih264_inter_pred_filters.c"
    "common/ih264_iquant_itrans_recon.c"
    "common/ih264_list.c"
    "common/ih264_luma_intra_pred_filters.c"
    "common/ih264_mem_fns.c"
    "common/ih264_padding.c"
    "common/ih264_resi_trans_quant.c"
    "common/ih264_trans_data.c"
    "common/ih264_weighted_pred.c"
    "common/ithread.c"
    "decoder/ih264d_api.c"
    "decoder/ih264d_bitstrm.c"
    "decoder/ih264d_cabac.c"
    "decoder/ih264d_cabac_init_tables.c"
    "decoder/ih264d_compute_bs.c"
    "decoder/ih264d_deblocking.c"
    "decoder/ih264d_dpb_mgr.c"
    "decoder/ih264d_format_conv.c"
    "decoder/ih264d_function_selector_generic.c"
    "decoder/ih264d_inter_pred.c"
    "decoder/ih264d_mb_utils.c"
    "decoder/ih264d_mvpred.c"
    "decoder/ih264d_nal.c"
    "decoder/ih264d_parse_bslice.c"
    "decoder/ih264d_parse_cabac.c"
    "decoder/ih264d_parse_cavlc.c"
    "decoder/ih264d_parse_headers.c"
    "decoder/ih264d_parse_islice.c"
    "decoder/ih264d_parse_mb_header.c"
    "decoder/ih264d_parse_pslice.c"
    "decoder/ih264d_parse_slice.c"
    "decoder/ih264d_process_bslice.c"
    "decoder/ih264d_process_intra_mb.c"
    "decoder/ih264d_process_pslice.c"
    "decoder/ih264d_quant_scaling.c"
    "decoder/ih264d_sei.c"
    "decoder/ih264d_tables.c"
    "decoder/ih264d_thread_compute_bs.c"
    "decoder/ih264d_thread_parse_decode.c"
    "decoder/ih264d_utils.c"
    "decoder/ih264d_vui.c"
)

if(CMAKE_OSX_ARCHITECTURES)
    set(IH264D_ARCHITECTURE ${CMAKE_OSX_ARCHITECTURES})
else()
    set(IH264D_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
endif()

message(STATUS "DEBUG: ih264d detected architecture is: ${IH264D_ARCHITECTURE}")

if(IH264D_ARCHITECTURE MATCHES "x86_64|amd64|AMD64|i386|i686")
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
        "common/x86/ih264_resi_trans_quant_sse42.c"
        "common/x86/ih264_weighted_pred_sse42.c"
        "decoder/x86/ih264d_function_selector.c"
        "decoder/x86/ih264d_function_selector_sse42.c"
        "decoder/x86/ih264d_function_selector_ssse3.c"
    )

elseif(IH264D_ARCHITECTURE MATCHES "aarch64|arm64|ARM64")
    set(LIBAVCDEC_ARM_INCLUDES "common/armv8" "decoder/arm")
    include_directories("common/" "decoder/" ${LIBAVCDEC_ARM_INCLUDES})

    if(MSVC)
        message(STATUS "MSVC ARM64 detected: Forcing portable C implementation.")
        target_sources(ih264d PRIVATE
            "decoder/arm/ih264d_function_selector.c"
            "decoder/arm/ih264d_function_selector_av8.c"
        )
        target_compile_definitions(ih264d PRIVATE PORTABLE_C ARCH_GENERIC DISABLE_NEON)
    else()
        enable_language(ASM)
        message(STATUS "ARM64 Clang/GCC detected: Enabling assembly optimization.")
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
            "common/armv8/ih264_padding_neon_av8.s"
            "common/armv8/ih264_resi_trans_quant_av8.s"
            "common/armv8/ih264_weighted_bi_pred_av8.s"
            "common/armv8/ih264_weighted_pred_av8.s"
            "decoder/arm/ih264d_function_selector_av8.c"
            "decoder/arm/ih264d_function_selector.c"
        )
        target_compile_definitions(ih264d PRIVATE ARMV8)
        set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp")
    endif()

    if(APPLE)
        target_sources(ih264d PRIVATE "common/armv8/macos_arm_symbol_aliases.s")
    endif()

else()
    message(FATAL_ERROR "ih264d unknown architecture: ${IH264D_ARCHITECTURE}")
endif()

if(MSVC)
    set_property(TARGET ih264d PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(ih264d PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/Oi> $<$<CONFIG:Release,RelWithDebInfo>:/Ot> "/GS-")
endif()
