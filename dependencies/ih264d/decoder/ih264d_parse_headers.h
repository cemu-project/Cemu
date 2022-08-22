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
 * Modified for use with Cemu emulator project
*/
#ifndef  _IH264D_PARSE_HEADERS_H_
#define  _IH264D_PARSE_HEADERS_H_
/*!
**************************************************************************
* \file ih264d_parse_headers.h
*
* \brief
*    Contains declarations high level syntax[above slice]
*    parsing routines
*
* \date
*    19/12/2002
*
* \author  AI
**************************************************************************
*/
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"
#include "ih264d_structs.h"
WORD32 ih264d_parse_nal_unit(iv_obj_t *dec_hdl,
                          ivd_video_decode_op_t *ps_dec_op,
                          UWORD8 *pu1_buf,
                          UWORD32 u4_length);

#endif /* _IH264D_PARSE_HEADERS_H_ */
