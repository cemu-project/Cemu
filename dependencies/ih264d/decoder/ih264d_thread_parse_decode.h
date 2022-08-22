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
/*
 * ih264d_thread_parse_decode.h
 *
 *  Created on: Feb 21, 2012
 *      Author: 100492
 */

#ifndef _IH264D_THREAD_PARSE_DECPDE_H_
#define _IH264D_THREAD_PARSE_DECPDE_H_
void ih264d_parse_tfr_nmb(dec_struct_t *ps_dec,
                          UWORD8 u1_mb_idx,
                          UWORD8 u1_num_mbs,
                          UWORD8 u1_num_mbs_next,
                          UWORD8 u1_tfr_n_mb,
                          UWORD8 u1_end_of_row);
void ih264d_decode_tfr_nmb(dec_struct_t *ps_dec,
                           UWORD8 u1_num_mbs,
                           UWORD8 u1_num_mbs_next,
                           UWORD8 u1_end_of_row);
WORD32 ih264d_decode_recon_tfr_nmb_thread(dec_struct_t * ps_dec,
                                          UWORD8 u1_num_mbs,
                                          UWORD8 u1_num_mbs_next,
                                          UWORD8 u1_end_of_row);
void ih264d_decode_picture_thread(dec_struct_t *ps_dec);
WORD32 ih264d_decode_slice_thread(dec_struct_t *ps_dec);



#endif /* _IH264D_THREAD_PARSE_DECPDE_H_ */
