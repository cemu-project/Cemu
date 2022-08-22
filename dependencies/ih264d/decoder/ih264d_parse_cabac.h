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

/*!
 ***************************************************************************
 * \file ih264d_parse_cabac.h
 *
 * \brief
 *    This file contains cabac Residual decoding routines.
 *
 * \date
 *    20/03/2003
 *
 * \author  NS
 ***************************************************************************
 */
#ifndef _IH264D_PARSE_CABAC_H_
#define _IH264D_PARSE_CABAC_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"

#define UCOFF_LEVEL  14


UWORD8 ih264d_read_coeff4x4_cabac(dec_bit_stream_t *ps_bitstrm,
                                  UWORD32 u4_ctxcat,
                                  bin_ctxt_model_t *ps_ctxt_sig_coeff,
                                  dec_struct_t *ps_dec,
                                  bin_ctxt_model_t *ps_ctxt_coded);

void ih264d_read_coeff8x8_cabac(dec_bit_stream_t *ps_bitstrm,
                                dec_struct_t *ps_dec,
                                dec_mb_info_t *ps_cur_mb_info);

UWORD32 cabac_parse_8x8block_transform8x8_set(WORD16 *pi2_coeff_block,
                                              dec_struct_t * ps_dec,
                                              UWORD8 *pu1_top_nnz,
                                              UWORD8 *pu1_left_nnz,
                                              dec_mb_info_t *ps_cur_mb_info);

#endif  /* _IH264D_PARSE_CABAC_H_ */
