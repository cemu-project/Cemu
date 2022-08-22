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
 *  ih264_deblk_edge_filters.h
 *
 * @brief
 *  This file contains declarations of functions used for deblocking
 *
 * @author
 *  Ittiam
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

#ifndef IH264_DEBLK_H_
#define IH264_DEBLK_H_

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

typedef void ih264_deblk_edge_bslt4_ft(UWORD8 *pu1_src,
                                       WORD32 src_strd,
                                       WORD32 alpha,
                                       WORD32 beta,
                                       UWORD32 u4_bs,
                                       const UWORD8 *pu1_cliptab );

typedef void ih264_deblk_edge_bs4_ft(UWORD8 *pu1_src,
                                     WORD32 src_strd,
                                     WORD32 alpha,
                                     WORD32 beta );

typedef void ih264_deblk_chroma_edge_bslt4_ft(UWORD8 *pu1_src,
                                              WORD32 src_strd,
                                              WORD32 alpha_cb,
                                              WORD32 beta_cb,
                                              WORD32 alpha_cr,
                                              WORD32 beta_cr,
                                              UWORD32 u4_bs,
                                              const UWORD8 *pu1_cliptab_cb,
                                              const UWORD8 *pu1_cliptab_cr);

typedef void ih264_deblk_chroma_edge_bs4_ft(UWORD8 *pu1_src,
                                            WORD32 src_strd,
                                            WORD32 alpha_cb,
                                            WORD32 beta_cb,
                                            WORD32 alpha_cr,
                                            WORD32 beta_cr);



ih264_deblk_edge_bs4_ft ih264_deblk_luma_horz_bs4;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4_mbaff;


ih264_deblk_edge_bs4_ft ih264_deblk_chroma_horz_bs4_bp;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_bp;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff_bp;


ih264_deblk_edge_bslt4_ft ih264_deblk_luma_horz_bslt4;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4_mbaff;


ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_bp;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_bp;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff_bp;

ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4_mbaff;

ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_mbaff;


/*A9*/
ih264_deblk_edge_bs4_ft ih264_deblk_luma_horz_bs4_a9;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4_a9;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4_mbaff_a9;


ih264_deblk_edge_bs4_ft ih264_deblk_chroma_horz_bs4_bp_a9;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_bp_a9;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff_bp_a9;


ih264_deblk_edge_bslt4_ft ih264_deblk_luma_horz_bslt4_a9;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4_a9;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4_mbaff_a9;


ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_bp_a9;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_bp_a9;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff_bp_a9;

ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4_a9;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4_a9;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff_a9;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4_mbaff_a9;

ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_a9;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_a9;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff_a9;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_mbaff_a9;

/*AV8*/
ih264_deblk_edge_bs4_ft ih264_deblk_luma_horz_bs4_av8;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4_av8;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4_mbaff_av8;


ih264_deblk_edge_bs4_ft ih264_deblk_chroma_horz_bs4_bp_av8;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_bp_av8;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff_bp_av8;


ih264_deblk_edge_bslt4_ft ih264_deblk_luma_horz_bslt4_av8;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4_av8;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4_mbaff_av8;


ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_bp_av8;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_bp_av8;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff_bp_av8;

ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4_av8;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4_av8;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff_av8;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4_mbaff_av8;

ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_av8;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_av8;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff_av8;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_mbaff_av8;

/*SSE3*/
ih264_deblk_edge_bs4_ft ih264_deblk_luma_horz_bs4_ssse3;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4_ssse3;
ih264_deblk_edge_bs4_ft ih264_deblk_luma_vert_bs4_mbaff_ssse3;


ih264_deblk_edge_bs4_ft ih264_deblk_chroma_horz_bs4_bp_ssse3;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_bp_ssse3;
ih264_deblk_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff_bp_ssse3;


ih264_deblk_edge_bslt4_ft ih264_deblk_luma_horz_bslt4_ssse3;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4_ssse3;
ih264_deblk_edge_bslt4_ft ih264_deblk_luma_vert_bslt4_mbaff_ssse3;


ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_bp_ssse3;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_bp_ssse3;
ih264_deblk_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff_bp_ssse3;

ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4_ssse3;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4_ssse3;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_vert_bs4_mbaff_ssse3;
ih264_deblk_chroma_edge_bs4_ft ih264_deblk_chroma_horz_bs4_mbaff_ssse3;

ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_ssse3;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_ssse3;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_vert_bslt4_mbaff_ssse3;
ih264_deblk_chroma_edge_bslt4_ft ih264_deblk_chroma_horz_bslt4_mbaff_ssse3;

#endif /* IH264_DEBLK_H_ */
