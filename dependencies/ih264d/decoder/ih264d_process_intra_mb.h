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
 **************************************************************************
 * \file ih264d_process_intra_mb.h
 *
 * \brief
 *    Contains routines that decode a I slice type
 *
 * Detailed_description
 *
 * \date
 *    07/07/2003
 *
 * \author  NS
 **************************************************************************
 */
#ifndef _IH264D_PROCESS_INTRA_MB_H_
#define _IH264D_PROCESS_INTRA_MB_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"

#define CHROMA_TO_LUMA_INTRA_MODE(x)   (x ^ ( (!(x & 0x01)) << 1))
#define MB_TYPE_TO_INTRA_16x16_MODE(x) ((x - 1) & 0x03)

UWORD32 ih264d_unpack_luma_coeff4x4_mb(dec_struct_t * ps_dec,
                                    dec_mb_info_t * ps_cur_mb_info,
                                    UWORD8 intra_flag);
void ih264d_unpack_chroma_coeff4x4_mb(dec_struct_t * ps_dec,
                                      dec_mb_info_t * ps_cur_mb_info);
UWORD32 ih264d_unpack_luma_coeff8x8_mb(dec_struct_t * ps_dec,
                                    dec_mb_info_t * ps_cur_mb_info);

WORD32 ih264d_read_intra_pred_modes(dec_struct_t *ps_dec,
                                    UWORD8 *pu1_prev_intra4x4_pred_mode_flag,
                                    UWORD8 *pu1_rem_intra4x4_pred_mode,
                                    UWORD32 u4_trans_form8x8);

WORD32 ih264d_process_intra_mb(dec_struct_t * ps_dec,
                               dec_mb_info_t * ps_cur_mb_info,
                               UWORD8 u1_mb_num);

#endif  /* _IH264D_PROCESS_INTRA_MB_H_ */

