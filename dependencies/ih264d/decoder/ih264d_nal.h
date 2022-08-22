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

#ifndef _IH264D_NAL_H_
#define _IH264D_NAL_H_

/*!
*************************************************************************
* \file ih264d_nal.h
*
* \brief
*    short_description
*
* Detailed_description
*
* \date
*    21/11/2002
*
* \author  AI
*************************************************************************
*/
#include <stdio.h>
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"

WORD32 ih264d_process_nal_unit(dec_bit_stream_t *ps_bitstrm,
                            UWORD8 *pu1_nal_unit,
                            UWORD32 u4_numbytes_in_nal_unit);
void ih264d_rbsp_to_sodb(dec_bit_stream_t *ps_bitstrm);
WORD32 ih264d_find_start_code(UWORD8 *pu1_buf,
                              UWORD32 u4_cur_pos,
                              UWORD32 u4_max_ofst,
                              UWORD32 *pu4_length_of_start_code,
                              UWORD32 *pu4_next_is_aud);


#endif /* _IH264D_NAL_H_ */
