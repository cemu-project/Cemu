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
*  ih264_weighted_pred.h
*
* @brief
*  Declarations of functions used for weighted prediction
*
* @author
*  Ittiam
*
* @par List of Functions:
*  -ih264_default_weighted_pred_luma
*  -ih264_default_weighted_pred_chroma
*  -ih264_weighted_pred_luma
*  -ih264_weighted_pred_chroma
*  -ih264_weighted_bi_pred_luma
*  -ih264_weighted_bi_pred_chroma
*  -ih264_default_weighted_pred_luma_a9q
*  -ih264_default_weighted_pred_chroma_a9q
*  -ih264_weighted_pred_luma_a9q
*  -ih264_weighted_pred_luma_a9q
*  -ih264_weighted_bi_pred_luma_a9q
*  -ih264_weighted_bi_pred_chroma_a9q
*  -ih264_default_weighted_pred_luma_av8
*  -ih264_default_weighted_pred_chroma_av8
*  -ih264_weighted_pred_luma_av8
*  -ih264_weighted_pred_chroma_av8
*  -ih264_weighted_bi_pred_luma_av8
*  -ih264_weighted_bi_pred_chroma_av8
*  -ih264_default_weighted_pred_luma_sse42
*  -ih264_default_weighted_pred_chroma_sse42
*  -ih264_weighted_pred_luma_sse42
*  -ih264_weighted_pred_chroma_sse42
*  -ih264_weighted_bi_pred_luma_sse42
*  -ih264_weighted_bi_pred_chroma_sse42
*
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef IH264_WEIGHTED_PRED_H_
#define IH264_WEIGHTED_PRED_H_

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
typedef void ih264_default_weighted_pred_ft(UWORD8 *puc_src1,
                                            UWORD8 *puc_src2,
                                            UWORD8 *puc_dst,
                                            WORD32 src_strd1,
                                            WORD32 src_strd2,
                                            WORD32 dst_strd,
                                            WORD32 ht,
                                            WORD32 wd);

typedef void ih264_weighted_pred_ft(UWORD8 *puc_src,
                                    UWORD8 *puc_dst,
                                    WORD32 src_strd,
                                    WORD32 dst_strd,
                                    WORD32 log_wd,
                                    WORD32 wt,
                                    WORD32 ofst,
                                    WORD32 ht,
                                    WORD32 wd);

typedef void ih264_weighted_bi_pred_ft(UWORD8 *puc_src1,
                                       UWORD8 *puc_src2,
                                       UWORD8 *puc_dst,
                                       WORD32 src_strd1,
                                       WORD32 src_strd2,
                                       WORD32 dst_strd,
                                       WORD32 log_wd,
                                       WORD32 wt1,
                                       WORD32 wt2,
                                       WORD32 ofst1,
                                       WORD32 ofst2,
                                       WORD32 ht,
                                       WORD32 wd);

/* No NEON Declarations */

ih264_default_weighted_pred_ft ih264_default_weighted_pred_luma;

ih264_default_weighted_pred_ft ih264_default_weighted_pred_chroma;

ih264_weighted_pred_ft ih264_weighted_pred_luma;

ih264_weighted_pred_ft ih264_weighted_pred_chroma;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_luma;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_chroma;

/* A9 NEON Declarations */

ih264_default_weighted_pred_ft ih264_default_weighted_pred_luma_a9q;

ih264_default_weighted_pred_ft ih264_default_weighted_pred_chroma_a9q;

ih264_weighted_pred_ft ih264_weighted_pred_luma_a9q;

ih264_weighted_pred_ft ih264_weighted_pred_chroma_a9q;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_luma_a9q;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_chroma_a9q;


/* AV8 NEON Declarations */

ih264_default_weighted_pred_ft ih264_default_weighted_pred_luma_av8;

ih264_default_weighted_pred_ft ih264_default_weighted_pred_chroma_av8;

ih264_weighted_pred_ft ih264_weighted_pred_luma_av8;

ih264_weighted_pred_ft ih264_weighted_pred_chroma_av8;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_luma_av8;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_chroma_av8;


/* SSE42 Intrinsic Declarations */

ih264_default_weighted_pred_ft ih264_default_weighted_pred_luma_sse42;

ih264_default_weighted_pred_ft ih264_default_weighted_pred_chroma_sse42;

ih264_weighted_pred_ft ih264_weighted_pred_luma_sse42;

ih264_weighted_pred_ft ih264_weighted_pred_chroma_sse42;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_luma_sse42;

ih264_weighted_bi_pred_ft ih264_weighted_bi_pred_chroma_sse42;

#endif /* IH264_WEIGHTED_PRED_H_ */

/** Nothing past this point */
