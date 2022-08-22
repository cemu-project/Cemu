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
 * \file ih264d_parse_islice.h
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

#ifndef _IH264D_PARSE_ISLICE_H_
#define _IH264D_PARSE_ISLICE_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_tables.h"

WORD32 ih264d_parse_residual4x4_cavlc(dec_struct_t * ps_dec,
                                      dec_mb_info_t *ps_cur_mb_info,
                                      UWORD8 u1_offset);
WORD32 ih264d_parse_residual4x4_cabac(dec_struct_t * ps_dec,
                                      dec_mb_info_t *ps_cur_mb_info,
                                      UWORD8 u1_offset);
WORD32 ih264d_parse_imb_cavlc(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_num,
                              UWORD8 u1_mb_type);
WORD32 ih264d_parse_imb_cabac(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_type);

WORD32 ih264d_parse_islice_data_cavlc(dec_struct_t * ps_dec,
                                      dec_slice_params_t * ps_slice,
                                      UWORD16 u2_first_mb_in_slice);
WORD32 ih264d_parse_islice_data_cabac(dec_struct_t * ps_dec,
                                      dec_slice_params_t * ps_slice,
                                      UWORD16 u2_first_mb_in_slice);
WORD32 ih264d_parse_pmb_cavlc(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_num,
                              UWORD8 u1_num_mbsNby2);
WORD32 ih264d_parse_pmb_cabac(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_num,
                              UWORD8 u1_num_mbsNby2);

WORD32 ih264d_parse_bmb_non_direct_cavlc(dec_struct_t * ps_dec,
                                       dec_mb_info_t * ps_cur_mb_info,
                                       UWORD8 u1_mb_num,
                                       UWORD8 u1_mbNumModNBy2);

WORD32 ih264d_parse_bmb_non_direct_cabac(dec_struct_t * ps_dec,
                                       dec_mb_info_t * ps_cur_mb_info,
                                       UWORD8 u1_mb_num,
                                       UWORD8 u1_mbNumModNBy2);

WORD32 ih264d_parse_bmb_cavlc(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_num,
                              UWORD8 u1_num_mbsNby2);

WORD32 ih264d_parse_bmb_cabac(dec_struct_t * ps_dec,
                              dec_mb_info_t * ps_cur_mb_info,
                              UWORD8 u1_mb_num,
                              UWORD8 u1_num_mbsNby2);

WORD32 ih264d_parse_inter_slice_data_cavlc(dec_struct_t * ps_dec,
                                           dec_slice_params_t * ps_slice,
                                           UWORD16 u2_first_mb_in_slice);

WORD32 ih264d_parse_inter_slice_data_cabac(dec_struct_t * ps_dec,
                                           dec_slice_params_t * ps_slice,
                                           UWORD16 u2_first_mb_in_slice);

WORD32 ParseBMb(dec_struct_t * ps_dec,
                dec_mb_info_t * ps_cur_mb_info,
                UWORD8 u1_mb_num,
                UWORD8 u1_num_mbsNby2);

WORD32 ih264d_parse_ipcm_mb(dec_struct_t * ps_dec,
                            dec_mb_info_t *ps_cur_mb_info,
                            UWORD8 u1_mbNum);
WORD32 ih264d_parse_islice(dec_struct_t *ps_dec,
                            UWORD16 u2_first_mb_in_slice);

#endif  /* _IH264D_PARSE_ISLICE_H_ */
