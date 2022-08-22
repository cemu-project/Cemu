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

#ifndef _IH264D_THREAD_COMPUTE_BS_H_
#define _IH264D_THREAD_COMPUTE_BS_H_
void ih264d_compute_bs_non_mbaff_thread(dec_struct_t * ps_dec,
                                        dec_mb_info_t * ps_cur_mb_info,
                                        UWORD32 u4_mb_num);

void ih264d_recon_deblk_thread(dec_struct_t *ps_dec);
void ih264d_check_mb_map_deblk(dec_struct_t *ps_dec,
                                    UWORD32 deblk_mb_grp,
                                    tfr_ctxt_t *ps_tfr_cxt,
                                    UWORD32 u4_check_mb_map);
#endif /* _IH264D_THREAD_COMPUTE_BS_H_ */
