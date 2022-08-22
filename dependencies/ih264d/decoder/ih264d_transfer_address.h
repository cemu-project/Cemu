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
#ifndef _IH264D_TRANSFER_ADDRESS_H_
#define _IH264D_TRANSFER_ADDRESS_H_

typedef struct
{
    UWORD8 *pu1_src_y;
    UWORD8 *pu1_src_u;
    UWORD8 *pu1_src_v;
    UWORD8 *pu1_dest_y;
    UWORD8 *pu1_dest_u;
    UWORD8 *pu1_dest_v;
    UWORD32 u4_inc_y[2];
    UWORD32 u4_inc_uv[2];
    UWORD16 u2_frm_wd_y;
    UWORD16 u2_frm_wd_uv;
    UWORD8 *pu1_mb_y;
    UWORD8 *pu1_mb_u;
    UWORD8 *pu1_mb_v;
    UWORD16 u2_mv_left_inc;
    UWORD16 u2_mv_top_left_inc;
    UWORD32 u4_y_inc;
    UWORD32 u4_uv_inc;

} tfr_ctxt_t;

#endif
