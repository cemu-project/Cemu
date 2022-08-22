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
#ifndef _IH264D_QUANT_SCALING_H_
#define _IH264D_QUANT_SCALING_H_
WORD32 ih264d_scaling_list(WORD16 *pi2_scaling_list,
                  WORD32 i4_size_of_scalinglist,
                  UWORD8 *pu1_use_default_scaling_matrix_flag,
                  dec_bit_stream_t *ps_bitstrm);


WORD32 ih264d_form_scaling_matrix_picture(dec_seq_params_t *ps_seq,
                                          dec_pic_params_t *ps_pic,
                                          dec_struct_t *ps_dec);

WORD32 ih264d_form_default_scaling_matrix(dec_struct_t *ps_dec);




#endif
