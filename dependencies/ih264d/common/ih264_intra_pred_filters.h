/******************************************************************************
 *
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/
/**
 *******************************************************************************
 * @file
 *  ih264_intra_pred_filters.h
 *
 * @brief
 *  Declarations of functions used for intra prediction
 *
 * @author
 *  Ittiam
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

#ifndef IH264_INTRA_PRED_FILTERS_H_

#define IH264_INTRA_PRED_FILTERS_H_

/*****************************************************************************/
/*  Macro Expansion                                                          */
/*****************************************************************************/

/*! Filter (1,2,1) i.e (a + 2b + c) / 4 */
#define FILT121(a,b,c) ((a + (b<<1) + c + 2)>>2)
/*! Filter (1,1) i.e (a + b) / 2 */
#define FILT11(a,b) ((a + b + 1)>>1)
/*****************************************************************************/
/*  Global Variables                                                        */
/*****************************************************************************/

/* Global variables used only in assembly files*/
extern const WORD8  ih264_gai1_intrapred_luma_plane_coeffs[];
extern const WORD8  ih264_gai1_intrapred_chroma_plane_coeffs1[];
extern const WORD8  ih264_gai1_intrapred_chroma_plane_coeffs2[];
extern const WORD8  ih264_gai1_intrapred_luma_8x8_horz_u[];

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/


typedef void ih264_intra_pred_ref_filtering_ft(UWORD8 *pu1_left,
                                               UWORD8 *pu1_topleft,
                                               UWORD8 *pu1_top,
                                               UWORD8 *pu1_dst,
                                               WORD32 left_strd,
                                               WORD32 ngbr_avail);

typedef void ih264_intra_pred_luma_ft(UWORD8 *pu1_src,
                                      UWORD8 *pu1_dst,
                                      WORD32 src_strd,
                                      WORD32 dst_strd,
                                      WORD32 ngbr_avail);

/* No Neon Definitions */

/* Luma 4x4 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_dc;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dl;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dr;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_r;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_d;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_l;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_u;

/* Luma 8x8 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_dc;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dl;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dr;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_r;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_d;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_l;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_u;

/* Luma 16x16 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_vert;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_horz;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_dc;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_plane;

/* Chroma 8x8 Intra pred filters */

typedef ih264_intra_pred_luma_ft ih264_intra_pred_chroma_ft;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_dc;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_horz;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_vert;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_plane;


ih264_intra_pred_ref_filtering_ft  ih264_intra_pred_luma_8x8_mode_ref_filtering;

/* A9 Definition */

/* Luma 4x4 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_dc_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dl_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dr_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_r_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_d_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_l_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_u_a9q;

/* Luma 8x8 Intra pred filters */

ih264_intra_pred_ref_filtering_ft  ih264_intra_pred_luma_8x8_mode_ref_filtering_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_dc_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dl_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dr_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_r_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_d_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_l_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_u_a9q;

/* Luma 16x16 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_vert_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_horz_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_dc_a9q;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_plane_a9q;

/* Chroma 8x8 Intra pred filters */

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_dc_a9q;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_horz_a9q;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_vert_a9q;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_plane_a9q;

/* X86 Intrinsic Definitions */

/* Luma 4x4 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_dc_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dl_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dr_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_r_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_d_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_l_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_u_ssse3;

/* Luma 8x8 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_dc_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dl_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dr_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_r_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_d_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_l_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_u_ssse3;

/* Luma 16x16 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_vert_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_horz_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_dc_ssse3;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_plane_ssse3;

/* Chroma 8x8 Intra pred filters */

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_dc_ssse3;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_horz_ssse3;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_vert_ssse3;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_plane_ssse3;

/* AV8 Definition */

/* Luma 4x4 Intra pred filters */
ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_dc_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dl_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_diag_dr_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_r_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_d_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_vert_l_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_4x4_mode_horz_u_av8;

/* Luma 8x8 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_dc_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dl_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_diag_dr_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_r_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_d_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_vert_l_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_8x8_mode_horz_u_av8;

/* Luma 16x16 Intra pred filters */

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_vert_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_horz_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_dc_av8;

ih264_intra_pred_luma_ft  ih264_intra_pred_luma_16x16_mode_plane_av8;

/* Chroma 8x8 Intra pred filters */

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_dc_av8;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_horz_av8;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_vert_av8;

ih264_intra_pred_chroma_ft ih264_intra_pred_chroma_8x8_mode_plane_av8;

#endif /* IH264_INTRA_PRED_FILTERS_H_ */
