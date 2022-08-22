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
 * \file ih264d_process_intra_mb.c
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

#include <string.h>
#include "ih264d_bitstrm.h"
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_mb_utils.h"
#include "ih264d_parse_slice.h"
#include "ih264d_process_intra_mb.h"
#include "ih264d_error_handler.h"
#include "ih264d_quant_scaling.h"
#include "ih264d_tables.h"

/*!
 **************************************************************************
 * \if Function name : ih264d_itrans_recon_luma_dc \endif
 *
 * \brief
 *    This function does InvTransform, scaling and reconstruction of Luma DC.
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
void ih264d_itrans_recon_luma_dc(dec_struct_t *ps_dec,
                                 WORD16* pi2_src,
                                 WORD16* pi2_coeff_block,
                                 const UWORD16 *pu2_weigh_mat)
{
    WORD32 i;
    WORD16 pi2_out[16];
    WORD32 pi4_tmp[16];
    WORD16 *pi2_out_ptr = &pi2_out[0];
    PROFILE_DISABLE_IQ_IT_RECON_RETURN()
    ps_dec->pf_ihadamard_scaling_4x4(pi2_src, pi2_out,
                                     ps_dec->pu2_quant_scale_y, pu2_weigh_mat,
                                     ps_dec->u1_qp_y_div6, pi4_tmp);
    for(i = 0; i < 4; i++)
    {
        pi2_coeff_block[0] = pi2_out_ptr[0];
        pi2_coeff_block[4 * 16] = pi2_out_ptr[4];
        pi2_coeff_block[8 * 16] = pi2_out_ptr[8];
        pi2_coeff_block[12 * 16] = pi2_out_ptr[12];

        pi2_out_ptr++; /* Point to next column */
        pi2_coeff_block += 16;
    }
}
/*!
 **************************************************************************
 * \if Function name : ih264d_read_intra_pred_modes \endif
 *
 * \brief
 *    Reads the intra pred mode related values of I4x4 MB from bitstream.
 *
 *    This function will read the prev intra pred mode flags and
 *    stores it in pu1_prev_intra4x4_pred_mode_flag. If the u4_flag
 *    indicates that most probable mode is not intra pred mode, then
 *    the rem_intra4x4_pred_mode is read and stored in
 *    pu1_rem_intra4x4_pred_mode array.
 *
 *
 * \return
 *    0 on success and Error code otherwise
 *
 **************************************************************************
 */
WORD32 ih264d_read_intra_pred_modes(dec_struct_t * ps_dec,
                                    UWORD8 * pu1_prev_intra4x4_pred_mode_flag,
                                    UWORD8 * pu1_rem_intra4x4_pred_mode,
                                    UWORD32 u4_trans_form8x8)
{
    WORD32 i4x4_luma_blk_idx = 0, i8x8_luma_blk_idx = 0;

    dec_bit_stream_t * ps_bitstrm = ps_dec->ps_bitstrm;

    if(!u4_trans_form8x8)
    {
        for(i4x4_luma_blk_idx = 0; i4x4_luma_blk_idx < 16; ++i4x4_luma_blk_idx)
        {
            UWORD32 u4_temp;
            SWITCHOFFTRACE;

            GETBIT(u4_temp, ps_bitstrm->u4_ofst, ps_bitstrm->pu4_buffer);
            *pu1_prev_intra4x4_pred_mode_flag = (UWORD8)u4_temp;
            if(!(*pu1_prev_intra4x4_pred_mode_flag))
            {
                GETBITS(u4_temp, ps_bitstrm->u4_ofst, ps_bitstrm->pu4_buffer, 3);

                *(pu1_rem_intra4x4_pred_mode) = (UWORD8)u4_temp;
            }

            pu1_prev_intra4x4_pred_mode_flag++;
            pu1_rem_intra4x4_pred_mode++;
        }
    }
    else
    {
        /**********************************************************************/
        /* prev_intra4x4_pred_modes to be interpreted as                      */
        /* prev_intra8x8_pred_modes in case of transform 8x8                  */
        /**********************************************************************/
        for(i8x8_luma_blk_idx = 0; i8x8_luma_blk_idx < 4; i8x8_luma_blk_idx++)
        {
            UWORD32 u4_temp;
            GETBIT(u4_temp, ps_bitstrm->u4_ofst, ps_bitstrm->pu4_buffer);
            *pu1_prev_intra4x4_pred_mode_flag = (UWORD8)u4_temp;
            if(!(*pu1_prev_intra4x4_pred_mode_flag))
            {
                GETBITS(u4_temp, ps_bitstrm->u4_ofst, ps_bitstrm->pu4_buffer, 3);

                (*pu1_rem_intra4x4_pred_mode) = (UWORD8)u4_temp;
            }
            pu1_prev_intra4x4_pred_mode_flag++;
            pu1_rem_intra4x4_pred_mode++;
        }
    }
    return (0);
}
WORD32 ih264d_unpack_coeff4x4_4x4blk(dec_struct_t * ps_dec,
                                   WORD16 *pi2_out_coeff_data,
                                   UWORD8 *pu1_inv_scan)
{
    tu_sblk4x4_coeff_data_t *ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_proc_tu_coeff_data;
    UWORD16 u2_sig_coeff_map = ps_tu_4x4->u2_sig_coeff_map;
    WORD32 idx = 0;
    WORD16 *pi2_coeff_data = &ps_tu_4x4->ai2_level[0];
    WORD32 dc_only_flag = 0;
    WORD32 num_coeff = 0;

    PROFILE_DISABLE_UNPACK_LUMA()
    while(u2_sig_coeff_map)
    {
        idx = CLZ(u2_sig_coeff_map);

        idx = 31 - idx;
        RESET_BIT(u2_sig_coeff_map,idx);

        idx = pu1_inv_scan[idx];
        pi2_out_coeff_data[idx] = *pi2_coeff_data++;
        num_coeff++;
    }

    if((num_coeff == 1) && (idx == 0))
    {
        dc_only_flag = 1;
    }

    {
        WORD32 offset;
        offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_4x4;
        offset = ALIGN4(offset);
        ps_dec->pv_proc_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_proc_tu_coeff_data + offset);
    }

    return dc_only_flag;
}

UWORD32 ih264d_unpack_coeff4x4_8x8blk(dec_struct_t * ps_dec,
                                   dec_mb_info_t * ps_cur_mb_info,
                                   UWORD16 ui2_luma_csbp,
                                   WORD16 *pi2_out_coeff_data)
{
    UWORD8 *pu1_inv_scan;
    UWORD8 u1_mb_field_decoding_flag = ps_cur_mb_info->u1_mb_field_decodingflag;
    UWORD8 u1_field_coding_flag = ps_cur_mb_info->ps_curmb->u1_mb_fld;
    UWORD32 u4_luma_dc_only_csbp = 0;
    WORD32 dc_only_flag = 0;

    PROFILE_DISABLE_UNPACK_LUMA()
    if(u1_field_coding_flag || u1_mb_field_decoding_flag)
    {
        pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan_fld;
    }
    else
    {
        pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan;
    }

    // sub 0
    if(ui2_luma_csbp & 0x1)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        dc_only_flag = ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);

        INSERT_BIT(u4_luma_dc_only_csbp, 0, dc_only_flag);
    }

    pi2_out_coeff_data += 16;
    // sub 1
    if(ui2_luma_csbp & 0x2)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        dc_only_flag = ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
        INSERT_BIT(u4_luma_dc_only_csbp, 1, dc_only_flag);
    }

    pi2_out_coeff_data += 16 + 32;
    // sub 2
    if(ui2_luma_csbp & 0x10)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        dc_only_flag = ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
        INSERT_BIT(u4_luma_dc_only_csbp, 4, dc_only_flag);
    }

    pi2_out_coeff_data += 16;
    // sub 3
    if(ui2_luma_csbp & 0x20)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        dc_only_flag = ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
        INSERT_BIT(u4_luma_dc_only_csbp, 5, dc_only_flag);
    }
    return u4_luma_dc_only_csbp;
}
WORD32 ih264d_unpack_coeff8x8_8x8blk_cavlc(dec_struct_t * ps_dec,
                                            dec_mb_info_t * ps_cur_mb_info,
                                            UWORD16 ui2_luma_csbp,
                                            WORD16 *pi2_out_coeff_data)
{
    UWORD8 *pu1_inv_scan;
    UWORD8 u1_mb_field_decoding_flag = ps_cur_mb_info->u1_mb_field_decodingflag;
    UWORD8 u1_field_coding_flag = ps_cur_mb_info->ps_curmb->u1_mb_fld;
    WORD32 dc_only_flag = 0;

    PROFILE_DISABLE_UNPACK_LUMA()
    if(ui2_luma_csbp & 0x33)
    {
        memset(pi2_out_coeff_data,0,64*sizeof(WORD16));
    }

    if(!u1_mb_field_decoding_flag)
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[0];
    }
    else
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[0];
    }
    // sub 0
    if(ui2_luma_csbp & 0x1)
    {
        dc_only_flag = ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }

    if(!u1_mb_field_decoding_flag)
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[1];
    }
    else
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[1];
    }
    // sub 1
    if(ui2_luma_csbp & 0x2)
    {
        dc_only_flag = 0;
        ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }

    if(!u1_mb_field_decoding_flag)
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[2];
    }
    else
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[2];
    }
    // sub 2
    if(ui2_luma_csbp & 0x10)
    {
        dc_only_flag = 0;
        ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }

    if(!u1_mb_field_decoding_flag)
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[3];
    }
    else
    {
        pu1_inv_scan =
                        (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[3];
    }
    // sub 3
    if(ui2_luma_csbp & 0x20)
    {
        dc_only_flag = 0;
        ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }
    return dc_only_flag;
}
void ih264d_unpack_coeff4x4_8x8blk_chroma(dec_struct_t * ps_dec,
                                          dec_mb_info_t * ps_cur_mb_info,
                                          UWORD16 ui2_chroma_csbp,
                                          WORD16 *pi2_out_coeff_data)
{
    UWORD8 *pu1_inv_scan;
    UWORD8 u1_mb_field_decoding_flag = ps_cur_mb_info->u1_mb_field_decodingflag;
    UWORD8 u1_field_coding_flag = ps_cur_mb_info->ps_curmb->u1_mb_fld;

    PROFILE_DISABLE_UNPACK_CHROMA()
    if(u1_field_coding_flag || u1_mb_field_decoding_flag)
    {
        pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan_fld;
    }
    else
    {
        pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan;
    }

    if(ui2_chroma_csbp & 0x1)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }
    pi2_out_coeff_data += 16;
    if(ui2_chroma_csbp & 0x2)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }

    pi2_out_coeff_data += 16;
    if(ui2_chroma_csbp & 0x4)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }

    pi2_out_coeff_data += 16;
    if(ui2_chroma_csbp & 0x8)
    {
        memset(pi2_out_coeff_data,0,16*sizeof(WORD16));
        ih264d_unpack_coeff4x4_4x4blk(ps_dec,
                                      pi2_out_coeff_data,
                                      pu1_inv_scan);
    }
}
UWORD32 ih264d_unpack_luma_coeff4x4_mb(dec_struct_t * ps_dec,
                                    dec_mb_info_t * ps_cur_mb_info,
                                    UWORD8 intra_flag)
{
    UWORD8 u1_mb_type = ps_cur_mb_info->u1_mb_type;
    UWORD16 ui2_luma_csbp = ps_cur_mb_info->u2_luma_csbp;
    UWORD8 *pu1_inv_scan = ps_dec->pu1_inv_scan;
    WORD16 *pi2_coeff_data = ps_dec->pi2_coeff_data;

    PROFILE_DISABLE_UNPACK_LUMA()
    if(!ps_cur_mb_info->u1_tran_form8x8)
    {
        UWORD32 u4_luma_dc_only_csbp = 0;
        UWORD32 u4_temp = 0;
        WORD16* pi2_dc_val = NULL;
        /*
         * Reserve the pointer to dc vals. The dc vals will be copied
         * after unpacking of ac vals since memset to 0 inside.
         */
        if(intra_flag && (u1_mb_type != I_4x4_MB))
        {
            if(CHECKBIT(ps_cur_mb_info->u1_yuv_dc_block_flag,0))
            {
                pi2_dc_val = (WORD16 *)ps_dec->pv_proc_tu_coeff_data;

                ps_dec->pv_proc_tu_coeff_data = (void *)(pi2_dc_val + 16);
            }
        }

        if(ui2_luma_csbp)
        {
            pi2_coeff_data = ps_dec->pi2_coeff_data;
            u4_temp = ih264d_unpack_coeff4x4_8x8blk(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);
            u4_luma_dc_only_csbp = u4_temp;

            pi2_coeff_data += 32;

            ui2_luma_csbp = ui2_luma_csbp >> 2;
            u4_temp = ih264d_unpack_coeff4x4_8x8blk(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);

            u4_luma_dc_only_csbp |= (u4_temp << 2);

            pi2_coeff_data += 32 + 64;

            ui2_luma_csbp = ui2_luma_csbp >> 6;
            u4_temp = ih264d_unpack_coeff4x4_8x8blk(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);

            u4_luma_dc_only_csbp |= (u4_temp << 8);

            pi2_coeff_data += 32;

            ui2_luma_csbp = ui2_luma_csbp >> 2;
            u4_temp = ih264d_unpack_coeff4x4_8x8blk(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);
            u4_luma_dc_only_csbp |= (u4_temp << 10);
        }

        if(pi2_dc_val != NULL)
        {
            WORD32 i;
            pi2_coeff_data = ps_dec->pi2_coeff_data;
            for(i = 0; i < 4; i++)
            {
                pi2_coeff_data[0] = pi2_dc_val[0];
                pi2_coeff_data[4 * 16] = pi2_dc_val[4];
                pi2_coeff_data[8 * 16] = pi2_dc_val[8];
                pi2_coeff_data[12 * 16] = pi2_dc_val[12];

                pi2_dc_val++; /* Point to next column */
                pi2_coeff_data += 16;
            }
            u4_luma_dc_only_csbp = ps_cur_mb_info->u2_luma_csbp ^ 0xFFFF;
        }
        return u4_luma_dc_only_csbp;
    }
    else
    {
        UWORD32 u4_luma_dc_only_cbp = 0;
        WORD32 dc_only_flag;
        if(ui2_luma_csbp)
        {
            pi2_coeff_data = ps_dec->pi2_coeff_data;
            dc_only_flag = ih264d_unpack_coeff8x8_8x8blk_cavlc(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);
            INSERT_BIT(u4_luma_dc_only_cbp, 0, dc_only_flag);

            pi2_coeff_data += 64;

            ui2_luma_csbp = ui2_luma_csbp >> 2;
            dc_only_flag = ih264d_unpack_coeff8x8_8x8blk_cavlc(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);

            INSERT_BIT(u4_luma_dc_only_cbp, 1, dc_only_flag);

            pi2_coeff_data += 64;

            ui2_luma_csbp = ui2_luma_csbp >> 6;
            dc_only_flag = ih264d_unpack_coeff8x8_8x8blk_cavlc(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);

            INSERT_BIT(u4_luma_dc_only_cbp, 2, dc_only_flag);

            pi2_coeff_data += 64;
            ui2_luma_csbp = ui2_luma_csbp >> 2;
            dc_only_flag = ih264d_unpack_coeff8x8_8x8blk_cavlc(ps_dec,
                                          ps_cur_mb_info,
                                          ui2_luma_csbp,
                                          pi2_coeff_data);
            INSERT_BIT(u4_luma_dc_only_cbp, 3, dc_only_flag);
        }
        return u4_luma_dc_only_cbp;
    }

}

void ih264d_unpack_chroma_coeff4x4_mb(dec_struct_t * ps_dec,
                                      dec_mb_info_t * ps_cur_mb_info)
{
    UWORD8 u1_mb_type = ps_cur_mb_info->u1_mb_type;
    UWORD16 ui2_chroma_csbp = ps_cur_mb_info->u2_chroma_csbp;
    UWORD8 *pu1_inv_scan = ps_dec->pu1_inv_scan;
    WORD16 *pi2_coeff_data = ps_dec->pi2_coeff_data;
    WORD32 i;
    WORD16 *pi2_dc_val_u = NULL;
    WORD16 *pi2_dc_val_v = NULL;

    PROFILE_DISABLE_UNPACK_CHROMA()
    if((ps_cur_mb_info->u1_cbp >> 4) == CBPC_ALLZERO)
        return;

    /*
     * Reserve the pointers to dc vals. The dc vals will be copied
     * after unpacking of ac vals since memset to 0 inside.
     */
    if(CHECKBIT(ps_cur_mb_info->u1_yuv_dc_block_flag,1))
    {
        pi2_dc_val_u = (WORD16 *)ps_dec->pv_proc_tu_coeff_data;

        ps_dec->pv_proc_tu_coeff_data = (void *)(pi2_dc_val_u + 4);
    }
    if(CHECKBIT(ps_cur_mb_info->u1_yuv_dc_block_flag,2))
    {
        pi2_dc_val_v = (WORD16 *)ps_dec->pv_proc_tu_coeff_data;

        ps_dec->pv_proc_tu_coeff_data = (void *)(pi2_dc_val_v + 4);
    }

    if((ps_cur_mb_info->u1_cbp >> 4) == CBPC_NONZERO)
    {
        pi2_coeff_data = ps_dec->pi2_coeff_data;
        ih264d_unpack_coeff4x4_8x8blk_chroma(ps_dec,
                                             ps_cur_mb_info,
                                             ui2_chroma_csbp,
                                             pi2_coeff_data);

        pi2_coeff_data += 64;
        ui2_chroma_csbp = ui2_chroma_csbp >> 4;
        ih264d_unpack_coeff4x4_8x8blk_chroma(ps_dec,
                                             ps_cur_mb_info,
                                             ui2_chroma_csbp,
                                             pi2_coeff_data);

    }

    pi2_coeff_data = ps_dec->pi2_coeff_data;
    if(pi2_dc_val_u != NULL)
    {
        pi2_coeff_data[0] = *pi2_dc_val_u++;
        pi2_coeff_data[1 * 16] = *pi2_dc_val_u++;
        pi2_coeff_data[2 * 16] = *pi2_dc_val_u++;
        pi2_coeff_data[3 * 16] = *pi2_dc_val_u++;
    }
    else
    {
        pi2_coeff_data[0] = 0;
        pi2_coeff_data[1 * 16] = 0;
        pi2_coeff_data[2 * 16] = 0;
        pi2_coeff_data[3 * 16] = 0;
    }
    pi2_coeff_data += 64;
    if(pi2_dc_val_v != NULL)
    {
        pi2_coeff_data[0] = *pi2_dc_val_v++;
        pi2_coeff_data[1 * 16] = *pi2_dc_val_v++;
        pi2_coeff_data[2 * 16] = *pi2_dc_val_v++;
        pi2_coeff_data[3 * 16] = *pi2_dc_val_v++;
    }
    else
    {
        pi2_coeff_data[0] = 0;
        pi2_coeff_data[1 * 16] = 0;
        pi2_coeff_data[2 * 16] = 0;
        pi2_coeff_data[3 * 16] = 0;
    }
}
UWORD32 ih264d_unpack_luma_coeff8x8_mb(dec_struct_t * ps_dec,
                                    dec_mb_info_t * ps_cur_mb_info)
{
    WORD32 blk_8x8_cnt;
    WORD16 *pi2_out_coeff_data = ps_dec->pi2_coeff_data;
    UWORD8 u1_field_coding_flag = ps_cur_mb_info->ps_curmb->u1_mb_fld;
    UWORD8 *pu1_inv_scan;
    UWORD32 u4_luma_dc_only_cbp = 0;

    PROFILE_DISABLE_UNPACK_LUMA()
    if(!u1_field_coding_flag)
    {
        /*******************************************************************/
        /* initializing inverse scan  matrices                             */
        /*******************************************************************/
        pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan_prog8x8_cabac;
    }
    else
    {
        /*******************************************************************/
        /* initializing inverse scan  matrices                             */
        /*******************************************************************/
        pu1_inv_scan = (UWORD8 *)gau1_ih264d_inv_scan_int8x8_cabac;
    }

    for(blk_8x8_cnt = 0; blk_8x8_cnt < 4; blk_8x8_cnt++)
    {
        if(CHECKBIT(ps_cur_mb_info->u1_cbp, blk_8x8_cnt))
        {
            tu_blk8x8_coeff_data_t *ps_tu_8x8 = (tu_blk8x8_coeff_data_t *)ps_dec->pv_proc_tu_coeff_data;
            UWORD32 u4_sig_coeff_map;
            WORD32 idx = 0;
            WORD16 *pi2_coeff_data = &ps_tu_8x8->ai2_level[0];
            WORD32 num_coeff = 0;

            /* memset 64 coefficient to zero */
            memset(pi2_out_coeff_data,0,64*sizeof(WORD16));

            u4_sig_coeff_map = ps_tu_8x8->au4_sig_coeff_map[1];

            while(u4_sig_coeff_map)
            {
                idx = CLZ(u4_sig_coeff_map);

                idx = 31 - idx;
                RESET_BIT(u4_sig_coeff_map,idx);

                idx = pu1_inv_scan[idx + 32];
                pi2_out_coeff_data[idx] = *pi2_coeff_data++;
                num_coeff++;
            }

            u4_sig_coeff_map = ps_tu_8x8->au4_sig_coeff_map[0];
            while(u4_sig_coeff_map)
            {
                idx = CLZ(u4_sig_coeff_map);

                idx = 31 - idx;
                RESET_BIT(u4_sig_coeff_map,idx);

                idx = pu1_inv_scan[idx];
                pi2_out_coeff_data[idx] = *pi2_coeff_data++;
                num_coeff++;
            }

            if((num_coeff == 1) && (idx == 0))
            {
                SET_BIT(u4_luma_dc_only_cbp,blk_8x8_cnt);
            }


            {
                WORD32 offset;
                offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_8x8;
                offset = ALIGN4(offset);
                ps_dec->pv_proc_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_proc_tu_coeff_data + offset);
            }
        }
        pi2_out_coeff_data += 64;
    }

    return u4_luma_dc_only_cbp;
}
/*!
 **************************************************************************
 * \if Function name : ih264d_process_intra_mb \endif
 *
 * \brief
 *    This function decodes an I MB. Intraprediction is carried out followed
 *    by InvTramsform. Both IntraPrediction and Reconstrucion are carried out
 *    row buffer itself.
 *
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */
WORD32 ih264d_process_intra_mb(dec_struct_t * ps_dec,
                               dec_mb_info_t * ps_cur_mb_info,
                               UWORD8 u1_mb_num)
{
    UWORD8 u1_mb_type = ps_cur_mb_info->u1_mb_type;
    UWORD8 uc_temp = ps_cur_mb_info->u1_mb_ngbr_availablity;
    UWORD8 u1_top_available = BOOLEAN(uc_temp & TOP_MB_AVAILABLE_MASK);
    UWORD8 u1_left_available = BOOLEAN(uc_temp & LEFT_MB_AVAILABLE_MASK);
    UWORD8 u1_use_top_right_mb = BOOLEAN(uc_temp & TOP_RIGHT_MB_AVAILABLE_MASK);
    UWORD8 u1_use_top_left_mb = BOOLEAN(uc_temp & TOP_LEFT_MB_AVAILABLE_MASK);
    UWORD8 uc_useTopMB = u1_top_available;
    UWORD16 u2_use_left_mb = u1_left_available;
    UWORD16 u2_use_left_mb_pack;
    UWORD8 *pu1_luma_pred_buffer;
    /* CHANGED CODE */
    UWORD8 *pu1_luma_rec_buffer;
    UWORD8 *puc_top;

    mb_neigbour_params_t *ps_left_mb;
    mb_neigbour_params_t *ps_top_mb;
    mb_neigbour_params_t *ps_top_right_mb;
    mb_neigbour_params_t *ps_curmb;

    UWORD16 u2_mbx = ps_cur_mb_info->u2_mbx;
    UWORD32 ui_pred_width, ui_rec_width;
    WORD16 *pi2_y_coeff;
    UWORD8 u1_mbaff, u1_topmb, u1_mb_field_decoding_flag;
    UWORD32 u4_num_pmbair;
    UWORD16 ui2_luma_csbp = ps_cur_mb_info->u2_luma_csbp;
    UWORD8 *pu1_yleft, *pu1_ytop_left;
    /* Chroma variables*/
    UWORD8 *pu1_top_u;
    UWORD8 *pu1_uleft;
    UWORD8 *pu1_u_top_left;
    /* CHANGED CODE */
    UWORD8 *pu1_mb_cb_rei1_buffer, *pu1_mb_cr_rei1_buffer;
    UWORD32 u4_recwidth_cr;
    /* CHANGED CODE */
    tfr_ctxt_t *ps_frame_buf = ps_dec->ps_frame_buf_ip_recon;
    UWORD32 u4_luma_dc_only_csbp = 0;
    UWORD32 u4_luma_dc_only_cbp = 0;

    UWORD8 *pu1_prev_intra4x4_pred_mode_data = (UWORD8 *)ps_dec->pv_proc_tu_coeff_data;                 //Pointer to keep track of intra4x4_pred_mode data in pv_proc_tu_coeff_data buffer
    u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    u1_topmb = ps_cur_mb_info->u1_topmb;
    u4_num_pmbair = (u1_mb_num >> u1_mbaff);


    /*--------------------------------------------------------------------*/
    /* Find the current MB's mb params                                    */
    /*--------------------------------------------------------------------*/
    u1_mb_field_decoding_flag = ps_cur_mb_info->u1_mb_field_decodingflag;

    ps_curmb = ps_cur_mb_info->ps_curmb;
    ps_top_mb = ps_cur_mb_info->ps_top_mb;
    ps_left_mb = ps_cur_mb_info->ps_left_mb;
    ps_top_right_mb = ps_cur_mb_info->ps_top_right_mb;

    /*--------------------------------------------------------------------*/
    /* Check whether neighbouring MB is Inter MB and                      */
    /* constrained intra pred is 1.                                       */
    /*--------------------------------------------------------------------*/
    u2_use_left_mb_pack = (u2_use_left_mb << 8) + u2_use_left_mb;

    if(ps_dec->ps_cur_pps->u1_constrained_intra_pred_flag)
    {
        UWORD8 u1_left = (UWORD8)u2_use_left_mb;

        uc_useTopMB = uc_useTopMB
                        && ((ps_top_mb->u1_mb_type != P_MB)
                                        && (ps_top_mb->u1_mb_type != B_MB));
        u2_use_left_mb = u2_use_left_mb
                        && ((ps_left_mb->u1_mb_type != P_MB)
                                        && (ps_left_mb->u1_mb_type != B_MB));

        u2_use_left_mb_pack = (u2_use_left_mb << 8) + u2_use_left_mb;
        if(u1_mbaff)
        {
            if(u1_mb_field_decoding_flag ^ ps_left_mb->u1_mb_fld)
            {
                u1_left = u1_left
                                && (((ps_left_mb + 1)->u1_mb_type != P_MB)
                                                && ((ps_left_mb + 1)->u1_mb_type
                                                                != B_MB));
                u2_use_left_mb = u2_use_left_mb && u1_left;
                if(u1_mb_field_decoding_flag)
                    u2_use_left_mb_pack = (u1_left << 8)
                                    + (u2_use_left_mb_pack & 0xff);
                else
                    u2_use_left_mb_pack = (u2_use_left_mb << 8)
                                    + (u2_use_left_mb);
            }
        }
        u1_use_top_right_mb =
                        u1_use_top_right_mb
                                        && ((ps_top_right_mb->u1_mb_type != P_MB)
                                                        && (ps_top_right_mb->u1_mb_type
                                                                        != B_MB));

        u1_use_top_left_mb =
                        u1_use_top_left_mb
                                        && ((ps_cur_mb_info->u1_topleft_mbtype != P_MB)
                                                        && (ps_cur_mb_info->u1_topleft_mbtype
                                                                        != B_MB));
    }

    /*********************Common pointer calculations *************************/
    /* CHANGED CODE */
    pu1_luma_pred_buffer = ps_dec->pu1_y;
    pu1_luma_rec_buffer = ps_frame_buf->pu1_dest_y + (u4_num_pmbair << 4);
    pu1_mb_cb_rei1_buffer = ps_frame_buf->pu1_dest_u
                    + (u4_num_pmbair << 3) * YUV420SP_FACTOR;
    pu1_mb_cr_rei1_buffer = ps_frame_buf->pu1_dest_v + (u4_num_pmbair << 3);
    ui_pred_width = MB_SIZE;
    ui_rec_width = ps_dec->u2_frm_wd_y << u1_mb_field_decoding_flag;
    u4_recwidth_cr = ps_dec->u2_frm_wd_uv << u1_mb_field_decoding_flag;
    /************* Current and top luma pointer *****************/

    if(u1_mbaff)
    {
        if(u1_topmb == 0)
        {
            pu1_luma_rec_buffer += (
                            u1_mb_field_decoding_flag ?
                                            (ui_rec_width >> 1) :
                                            (ui_rec_width << 4));
            pu1_mb_cb_rei1_buffer += (
                            u1_mb_field_decoding_flag ?
                                            (u4_recwidth_cr >> 1) :
                                            (u4_recwidth_cr << 3));
            pu1_mb_cr_rei1_buffer += (
                            u1_mb_field_decoding_flag ?
                                            (u4_recwidth_cr >> 1) :
                                            (u4_recwidth_cr << 3));
        }
    }

    /* CHANGED CODE */
    if(ps_dec->u4_use_intrapred_line_copy == 1)
    {
        puc_top = ps_dec->pu1_prev_y_intra_pred_line + (ps_cur_mb_info->u2_mbx << 4);
        pu1_top_u = ps_dec->pu1_prev_u_intra_pred_line
                        + (ps_cur_mb_info->u2_mbx << 3) * YUV420SP_FACTOR;
    }
    else
    {
        puc_top = pu1_luma_rec_buffer - ui_rec_width;
        pu1_top_u = pu1_mb_cb_rei1_buffer - u4_recwidth_cr;
    }
    /* CHANGED CODE */

    /************* Left pointer *****************/
    pu1_yleft = pu1_luma_rec_buffer - 1;
    pu1_uleft = pu1_mb_cb_rei1_buffer - 1 * YUV420SP_FACTOR;

    /**************Top Left pointer calculation**********/
    pu1_ytop_left = puc_top - 1;
    pu1_u_top_left = pu1_top_u - 1 * YUV420SP_FACTOR;

    /* CHANGED CODE */
    PROFILE_DISABLE_INTRA_PRED()
    {
        pu1_prev_intra4x4_pred_mode_data = (UWORD8 *)ps_dec->pv_proc_tu_coeff_data;
        if(u1_mb_type == I_4x4_MB && ps_cur_mb_info->u1_tran_form8x8 == 0)
        {
            ps_dec->pv_proc_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_proc_tu_coeff_data + 32);

        }
        else if (u1_mb_type == I_4x4_MB && ps_cur_mb_info->u1_tran_form8x8 == 1)
        {
            ps_dec->pv_proc_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_proc_tu_coeff_data + 8);
        }
    }
    if(!ps_cur_mb_info->u1_tran_form8x8)
    {
        u4_luma_dc_only_csbp = ih264d_unpack_luma_coeff4x4_mb(ps_dec,
                                       ps_cur_mb_info,
                                       1);
    }
    else
    {
        if(!ps_dec->ps_cur_pps->u1_entropy_coding_mode)
        {
            u4_luma_dc_only_cbp = ih264d_unpack_luma_coeff4x4_mb(ps_dec,
                                           ps_cur_mb_info,
                                           1);
        }
        else
        {
            u4_luma_dc_only_cbp = ih264d_unpack_luma_coeff8x8_mb(ps_dec,
                                           ps_cur_mb_info);
        }
    }

    pi2_y_coeff = ps_dec->pi2_coeff_data;

    if(u1_mb_type != I_4x4_MB)
    {
        UWORD8 u1_intrapred_mode = MB_TYPE_TO_INTRA_16x16_MODE(u1_mb_type);
        /*--------------------------------------------------------------------*/
        /* 16x16 IntraPrediction                                              */
        /*--------------------------------------------------------------------*/
        {
            UWORD8 u1_packed_modes = (u1_top_available << 1)
                            + u1_left_available;
            UWORD8 u1_err_code =
                            (u1_intrapred_mode & 1) ?
                                            u1_intrapred_mode :
                                            (u1_intrapred_mode ^ 2);

            if((u1_err_code & u1_packed_modes) ^ u1_err_code)
            {
                u1_intrapred_mode = 0;
                ps_dec->i4_error_code = ERROR_INTRAPRED;
            }
        }
        {
            /* Align the size to multiple of 8, so that SIMD functions
               can read 64 bits at a time. Only 33 bytes are actaully used */
            UWORD8 au1_ngbr_pels[40];
            /* Get neighbour pixels */
            /* left pels */
            if(u2_use_left_mb)
            {
                WORD32 i;
                for(i = 0; i < 16; i++)
                    au1_ngbr_pels[16 - 1 - i] = pu1_yleft[i * ui_rec_width];
            }
            else
            {
                memset(au1_ngbr_pels, 0, 16);
            }

            /* top left pels */
            au1_ngbr_pels[16] = *pu1_ytop_left;

            /* top pels */
            if(uc_useTopMB)
            {
                memcpy(au1_ngbr_pels + 16 + 1, puc_top, 16);
            }
            else
            {
                memset(au1_ngbr_pels + 16 + 1, 0, 16);
            }
            PROFILE_DISABLE_INTRA_PRED()
            ps_dec->apf_intra_pred_luma_16x16[u1_intrapred_mode](
                            au1_ngbr_pels, pu1_luma_rec_buffer, 1, ui_rec_width,
                            ((uc_useTopMB << 2) | u2_use_left_mb));
        }
        {
            UWORD32 i;
            WORD16 ai2_tmp[16];
            for(i = 0; i < 16; i++)
            {
                WORD16 *pi2_level = pi2_y_coeff + (i << 4);
                UWORD8 *pu1_pred_sblk = pu1_luma_rec_buffer
                                + ((i & 0x3) * BLK_SIZE)
                                + (i >> 2) * (ui_rec_width << 2);
                PROFILE_DISABLE_IQ_IT_RECON()
                {
                    if(CHECKBIT(ps_cur_mb_info->u2_luma_csbp, i))
                    {
                        ps_dec->pf_iquant_itrans_recon_luma_4x4(
                                        pi2_level,
                                        pu1_pred_sblk,
                                        pu1_pred_sblk,
                                        ui_rec_width,
                                        ui_rec_width,
                                        gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qp_rem6],
                                        (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[0],
                                        ps_cur_mb_info->u1_qp_div6, ai2_tmp, 1,
                                        pi2_level);
                    }
                    else if((CHECKBIT(u4_luma_dc_only_csbp, i)) && pi2_level[0] != 0)
                    {
                        ps_dec->pf_iquant_itrans_recon_luma_4x4_dc(
                                        pi2_level,
                                        pu1_pred_sblk,
                                        pu1_pred_sblk,
                                        ui_rec_width,
                                        ui_rec_width,
                                        gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qp_rem6],
                                        (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[0],
                                        ps_cur_mb_info->u1_qp_div6, ai2_tmp, 1,
                                        pi2_level);
                    }
                }
            }
        }
    }
    else if(!ps_cur_mb_info->u1_tran_form8x8)
    {
        UWORD8 u1_is_left_sub_block, u1_is_top_sub_block = uc_useTopMB;
        UWORD8 u1_sub_blk_x, u1_sub_blk_y, u1_sub_mb_num;
        WORD8 i1_top_pred_mode;
        WORD8 i1_left_pred_mode;
        UWORD8 *pu1_top, *pu1_left, *pu1_top_left, *pu1_top_right;
        WORD8 *pi1_cur_pred_mode, *pi1_left_pred_mode, *pc_topPredMode;
        UWORD16 ui2_left_pred_buf_width = 0xffff;
        WORD8 i1_intra_pred;
        UWORD8 *pu1_prev_intra4x4_pred_mode_flag = pu1_prev_intra4x4_pred_mode_data;
        UWORD8 *pu1_rem_intra4x4_pred_mode = pu1_prev_intra4x4_pred_mode_data + 16;
        WORD16 *pi2_y_coeff1;
        UWORD8 u1_cur_sub_block;
        UWORD16 ui2_top_rt_mask;

        /*--------------------------------------------------------------------*/
        /* 4x4 IntraPrediction                                                */
        /*--------------------------------------------------------------------*/
        /* Calculation of Top Right subblock mask                             */
        /*                                                                    */
        /* (a) Set it to default mask                                         */
        /*     [It has 0 for sublocks which will never have top-right sub block] */
        /*                                                                    */
        /* (b) If top MB is not available                                     */
        /*      Clear the bits of the first row sub blocks                    */
        /*                                                                    */
        /* (c) Set/Clear bit for top-right sublock of MB                      */
        /*      [5 sub-block in decoding order] based on TOP RIGHT MB availablity */
        /*--------------------------------------------------------------------*/

        pu1_top = puc_top;

        ui2_top_rt_mask = (u1_use_top_right_mb << 3) | (0x5750);
        if(uc_useTopMB)
            ui2_top_rt_mask |= 0x7;

        /*Top Related initialisations*/


        pi1_cur_pred_mode = ps_cur_mb_info->ps_curmb->pi1_intrapredmodes;
        pc_topPredMode = ps_cur_mb_info->ps_top_mb->pi1_intrapredmodes;
        /*--------------------------------------
         if(u1_mbaff)
         {

         pi1_cur_pred_mode += (u2_mbx << 2);
         pc_topPredMode = pi1_cur_pred_mode + ps_cur_mb_info->i1_offset;
         pi1_cur_pred_mode += (u1_topmb) ? 0: 4;
         }*/

        if(u1_top_available)
        {
            if(ps_top_mb->u1_mb_type == I_4x4_MB)
                *(WORD32*)pi1_cur_pred_mode = *(WORD32*)pc_topPredMode;
            else
                *(WORD32*)pi1_cur_pred_mode =
                                (uc_useTopMB) ? DC_DC_DC_DC : NOT_VALID;
        }
        else
            *(WORD32*)pi1_cur_pred_mode = NOT_VALID;
        /* CHANGED CODE */

        /* CHANGED CODE */

        /*Left Related initialisations*/
        pi1_left_pred_mode = ps_dec->pi1_left_pred_mode;
        if(!u1_mbaff)
        {

            if(u1_left_available)
            {

                if(ps_left_mb->u1_mb_type != I_4x4_MB)
                    *(WORD32*)pi1_left_pred_mode =
                                    (u2_use_left_mb_pack) ?
                                    DC_DC_DC_DC :
                                                            NOT_VALID;

            }
            else
            {

                *(WORD32*)pi1_left_pred_mode = NOT_VALID;
            }

        }
        else
        {
            UWORD8 u1_curMbfld = ps_cur_mb_info->u1_mb_field_decodingflag;
            UWORD8 u1_leftMbfld = ps_left_mb->u1_mb_fld;

            if(u1_curMbfld ^ u1_leftMbfld)
            {

                if(u1_topmb
                                | ((u1_topmb == 0)
                                                && ((ps_curmb - 1)->u1_mb_type
                                                                != I_4x4_MB)))
                {
                    if(u1_left_available)
                    {
                        if(ps_left_mb->u1_mb_type != I_4x4_MB)
                        {
                            if(CHECKBIT(u2_use_left_mb_pack,0) == 0)
                                *(WORD32*)pi1_left_pred_mode = NOT_VALID;
                            else
                                *(WORD32*)pi1_left_pred_mode = DC_DC_DC_DC;
                        }
                    }
                    else
                        *(WORD32*)pi1_left_pred_mode = NOT_VALID;

                    if(u1_curMbfld)
                    {
                        if(u1_left_available)
                        {
                            if((ps_left_mb + 1)->u1_mb_type != I_4x4_MB)
                            {
                                if(u2_use_left_mb_pack >> 8)
                                    *(WORD32*)(pi1_left_pred_mode + 4) =
                                                    DC_DC_DC_DC;
                                else
                                    *(WORD32*)(pi1_left_pred_mode + 4) =
                                                    NOT_VALID;
                            }
                        }
                        else
                            *(WORD32*)(pi1_left_pred_mode + 4) = NOT_VALID;
                        pi1_left_pred_mode[1] = pi1_left_pred_mode[2];
                        pi1_left_pred_mode[2] = pi1_left_pred_mode[4];
                        pi1_left_pred_mode[3] = pi1_left_pred_mode[6];
                        *(WORD32*)(pi1_left_pred_mode + 4) =
                                        *(WORD32*)pi1_left_pred_mode;
                    }
                    else
                    {

                        pi1_left_pred_mode[7] = pi1_left_pred_mode[3];
                        pi1_left_pred_mode[6] = pi1_left_pred_mode[3];
                        pi1_left_pred_mode[5] = pi1_left_pred_mode[2];
                        pi1_left_pred_mode[4] = pi1_left_pred_mode[2];
                        pi1_left_pred_mode[3] = pi1_left_pred_mode[1];
                        pi1_left_pred_mode[2] = pi1_left_pred_mode[1];
                        pi1_left_pred_mode[1] = pi1_left_pred_mode[0];
                    }
                }
                pi1_left_pred_mode += (u1_topmb) ? 0 : 4;
            }
            else
            {

                pi1_left_pred_mode += (u1_topmb) ? 0 : 4;
                if(u1_left_available)
                {

                    if(ps_left_mb->u1_mb_type != I_4x4_MB)
                        *(WORD32*)pi1_left_pred_mode =
                                        (u2_use_left_mb_pack) ?
                                        DC_DC_DC_DC :
                                                                NOT_VALID;
                }
                else
                    *(WORD32*)pi1_left_pred_mode = NOT_VALID;
            }
        }
        /* One time pointer initialisations*/
        pi2_y_coeff1 = pi2_y_coeff;
        pu1_top_left = pu1_ytop_left;

        /* Scan the sub-blocks in Raster Scan Order */
        for(u1_sub_mb_num = 0; u1_sub_mb_num < 16; u1_sub_mb_num++)
        {
            /* Align the size to multiple of 8, so that SIMD functions
               can read 64 bits at a time. Only 13 bytes are actaully used */
            UWORD8 au1_ngbr_pels[16];

            u1_sub_blk_x = u1_sub_mb_num & 0x3;
            u1_sub_blk_y = u1_sub_mb_num >> 2;
            i1_top_pred_mode = pi1_cur_pred_mode[u1_sub_blk_x];
            i1_left_pred_mode = pi1_left_pred_mode[u1_sub_blk_y];
            u1_use_top_right_mb = (!!CHECKBIT(ui2_top_rt_mask, u1_sub_mb_num));

            /*********** left subblock availability**********/
            if(u1_sub_blk_x)
                u1_is_left_sub_block = 1;
            else
                u1_is_left_sub_block =
                                (u1_sub_blk_y < 2) ?
                                                (CHECKBIT(u2_use_left_mb_pack,
                                                          0)) :
                                                (u2_use_left_mb_pack >> 8);

            /* CHANGED CODE */
            if(u1_sub_blk_y)
                u1_is_top_sub_block = 1;

            /* CHANGED CODE */
            /***************** Top *********************/
            if(ps_dec->u4_use_intrapred_line_copy == 1)
            {

                if(u1_sub_blk_y)
                    pu1_top = pu1_luma_rec_buffer - ui_rec_width;
                else
                    pu1_top = puc_top + (u1_sub_blk_x << 2);
            }
            else
            {
                pu1_top = pu1_luma_rec_buffer - ui_rec_width;
            }
            /***************** Top Right *********************/
            pu1_top_right = pu1_top + 4;
            /***************** Top Left *********************/
            pu1_top_left = pu1_top - 1;
            /***************** Left *********************/
            pu1_left = pu1_luma_rec_buffer - 1;
            /* CHANGED CODE */

            /*---------------------------------------------------------------*/
            /* Calculation of Intra prediction mode                          */
            /*---------------------------------------------------------------*/
            i1_intra_pred = ((i1_left_pred_mode < 0) | (i1_top_pred_mode < 0)) ?
                            DC : MIN(i1_left_pred_mode, i1_top_pred_mode);
            {
                UWORD8 u1_packed_modes = (u1_is_top_sub_block << 1)
                                + u1_is_left_sub_block;
                UWORD8 *pu1_intra_err_codes =
                                (UWORD8 *)gau1_ih264d_intra_pred_err_code;
                UWORD8 uc_b2b0 = ((u1_sub_mb_num & 4) >> 1) | (u1_sub_mb_num & 1);
                UWORD8 uc_b3b1 = ((u1_sub_mb_num & 8) >> 2)
                                | ((u1_sub_mb_num & 2) >> 1);

                u1_cur_sub_block = (uc_b3b1 << 2) + uc_b2b0;
                PROFILE_DISABLE_INTRA_PRED()
                if(!pu1_prev_intra4x4_pred_mode_flag[u1_cur_sub_block])
                {
                    i1_intra_pred =
                                    pu1_rem_intra4x4_pred_mode[u1_cur_sub_block]
                                                    + (pu1_rem_intra4x4_pred_mode[u1_cur_sub_block]
                                                                    >= i1_intra_pred);
                }
                i1_intra_pred = CLIP3(0, 8, i1_intra_pred);
                {
                    UWORD8 u1_err_code = pu1_intra_err_codes[i1_intra_pred];

                    if((u1_err_code & u1_packed_modes) ^ u1_err_code)
                     {
                        i1_intra_pred = 0;
                        ps_dec->i4_error_code = ERROR_INTRAPRED;
                     }

                }
            }
            {
                /* Get neighbour pixels */
                /* left pels */
                if(u1_is_left_sub_block)
                {
                    WORD32 i;
                    for(i = 0; i < 4; i++)
                        au1_ngbr_pels[4 - 1 - i] = pu1_left[i * ui_rec_width];
                }
                else
                {
                    memset(au1_ngbr_pels, 0, 4);
                }

                /* top left pels */
                au1_ngbr_pels[4] = *pu1_top_left;

                /* top pels */
                if(u1_is_top_sub_block)
                {
                    memcpy(au1_ngbr_pels + 4 + 1, pu1_top, 4);
                }
                else
                {
                    memset(au1_ngbr_pels + 4 + 1, 0, 4);
                }

                /* top right pels */
                if(u1_use_top_right_mb)
                {
                    memcpy(au1_ngbr_pels + 4 * 2 + 1, pu1_top_right, 4);
                }
                else if(u1_is_top_sub_block)
                {
                    memset(au1_ngbr_pels + 4 * 2 + 1, au1_ngbr_pels[4 * 2], 4);
                }
            }
            PROFILE_DISABLE_INTRA_PRED()
            ps_dec->apf_intra_pred_luma_4x4[i1_intra_pred](
                            au1_ngbr_pels, pu1_luma_rec_buffer, 1,
                            ui_rec_width,
                            ((u1_is_top_sub_block << 2) | u1_is_left_sub_block));

            /* CHANGED CODE */
            if(CHECKBIT(ui2_luma_csbp, u1_sub_mb_num))
            {
                WORD16 ai2_tmp[16];
                PROFILE_DISABLE_IQ_IT_RECON()
                {
                    if(CHECKBIT(u4_luma_dc_only_csbp, u1_sub_mb_num))
                    {
                        ps_dec->pf_iquant_itrans_recon_luma_4x4_dc(
                                        pi2_y_coeff1,
                                        pu1_luma_rec_buffer,
                                        pu1_luma_rec_buffer,
                                        ui_rec_width,
                                        ui_rec_width,
                                        gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qp_rem6],
                                        (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[0],
                                        ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                        NULL);
                    }
                    else
                    {
                        ps_dec->pf_iquant_itrans_recon_luma_4x4(
                                        pi2_y_coeff1,
                                        pu1_luma_rec_buffer,
                                        pu1_luma_rec_buffer,
                                        ui_rec_width,
                                        ui_rec_width,
                                        gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qp_rem6],
                                        (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[0],
                                        ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                        NULL);
                    }
                }

            }

            /*---------------------------------------------------------------*/
            /* Update sub block number                                       */
            /*---------------------------------------------------------------*/
            pi2_y_coeff1 += 16;
            pu1_luma_rec_buffer +=
                            (u1_sub_blk_x == 3) ? (ui_rec_width << 2) - 12 : 4;
            pu1_luma_pred_buffer +=
                            (u1_sub_blk_x == 3) ? (ui_pred_width << 2) - 12 : 4;
            /* CHANGED CODE */
            pi1_cur_pred_mode[u1_sub_blk_x] = i1_intra_pred;
            pi1_left_pred_mode[u1_sub_blk_y] = i1_intra_pred;
        }
    }
    else if((u1_mb_type == I_4x4_MB) && (ps_cur_mb_info->u1_tran_form8x8 == 1))
    {
        UWORD8 u1_is_left_sub_block, u1_is_top_sub_block = uc_useTopMB;
        UWORD8 u1_sub_blk_x, u1_sub_blk_y, u1_sub_mb_num;
        WORD8 i1_top_pred_mode;
        WORD8 i1_left_pred_mode;
        UWORD8 *pu1_top, *pu1_left, *pu1_top_left;
        WORD8 *pi1_cur_pred_mode, *pi1_left_pred_mode, *pc_topPredMode;
        UWORD16 ui2_left_pred_buf_width = 0xffff;
        WORD8 i1_intra_pred;
        UWORD8 *pu1_prev_intra4x4_pred_mode_flag = pu1_prev_intra4x4_pred_mode_data;
        UWORD8 *pu1_rem_intra4x4_pred_mode = pu1_prev_intra4x4_pred_mode_data + 4;
        WORD16 *pi2_y_coeff1;
        UWORD16 ui2_top_rt_mask;
        UWORD32 u4_4x4_left_offset = 0;

        /*--------------------------------------------------------------------*/
        /* 8x8 IntraPrediction                                                */
        /*--------------------------------------------------------------------*/
        /* Calculation of Top Right subblock mask                             */
        /*                                                                    */
        /* (a) Set it to default mask                                         */
        /*  [It has 0 for sublocks which will never have top-right sub block] */
        /*                                                                    */
        /* (b) If top MB is not available                                     */
        /*      Clear the bits of the first row sub blocks                    */
        /*                                                                    */
        /* (c) Set/Clear bit for top-right sublock of MB                      */
        /*  [5 sub-block in decoding order] based on TOP RIGHT MB availablity */
        /*                                                                    */
        /* ui2_top_rt_mask: marks availibility of top right(neighbour)         */
        /* in the 8x8 Block ordering                                          */
        /*                                                                    */
        /*      tr0   tr1                                                     */
        /*   0    1   tr3                                                     */
        /*   2    3                                                           */
        /*                                                                    */
        /*  Top rights for 0 is in top MB                                     */
        /*  top right of 1 will be in top right MB                            */
        /*  top right of 3 in right MB and hence not available                */
        /*  This corresponds to ui2_top_rt_mask  having default value 0x4      */
        /*--------------------------------------------------------------------*/

        ui2_top_rt_mask = (u1_use_top_right_mb << 1) | (0x4);

        if(uc_useTopMB)
        {
            ui2_top_rt_mask |= 0x1;
        }

        /* Top Related initialisations */
        pi1_cur_pred_mode = ps_cur_mb_info->ps_curmb->pi1_intrapredmodes;
        pc_topPredMode = ps_cur_mb_info->ps_top_mb->pi1_intrapredmodes;
        /*
         if(u1_mbaff)
         {
         pi1_cur_pred_mode += (u2_mbx << 2);
         pc_topPredMode = pi1_cur_pred_mode + ps_cur_mb_info->i1_offset;
         pi1_cur_pred_mode += (u1_topmb) ? 0: 4;
         }
         */
        if(u1_top_available)
        {
            if(ps_top_mb->u1_mb_type == I_4x4_MB)
            {
                *(WORD32*)pi1_cur_pred_mode = *(WORD32*)pc_topPredMode;
            }
            else
            {
                *(WORD32*)pi1_cur_pred_mode =
                                (uc_useTopMB) ? DC_DC_DC_DC : NOT_VALID;
            }
        }
        else
        {
            *(WORD32*)pi1_cur_pred_mode = NOT_VALID;
        }

        pu1_top = puc_top - 8;

        /*Left Related initialisations*/
        pi1_left_pred_mode = ps_dec->pi1_left_pred_mode;

        if(!u1_mbaff)
        {
            if(u1_left_available)
            {
                if(ps_left_mb->u1_mb_type != I_4x4_MB)
                {
                    *(WORD32*)pi1_left_pred_mode =
                                    (u2_use_left_mb_pack) ?
                                    DC_DC_DC_DC :
                                                            NOT_VALID;
                }
            }
            else
            {
                *(WORD32*)pi1_left_pred_mode = NOT_VALID;
            }
        }
        else
        {
            UWORD8 u1_curMbfld = ps_cur_mb_info->u1_mb_field_decodingflag;

            UWORD8 u1_leftMbfld = ps_left_mb->u1_mb_fld;

            if((!u1_curMbfld) && (u1_leftMbfld))
            {
                u4_4x4_left_offset = 1;
            }

            if(u1_curMbfld ^ u1_leftMbfld)
            {

                if(u1_topmb
                                | ((u1_topmb == 0)
                                                && ((ps_curmb - 1)->u1_mb_type
                                                                != I_4x4_MB)))

                {
                    if(u1_left_available)
                    {
                        if(ps_left_mb->u1_mb_type != I_4x4_MB)
                        {
                            if(CHECKBIT(u2_use_left_mb_pack,0) == 0)
                            {
                                *(WORD32*)pi1_left_pred_mode = NOT_VALID;
                            }
                            else
                            {
                                *(WORD32*)pi1_left_pred_mode = DC_DC_DC_DC;
                            }
                        }
                    }
                    else
                    {
                        *(WORD32*)pi1_left_pred_mode = NOT_VALID;
                    }

                    if(u1_curMbfld)
                    {
                        if(u1_left_available)
                        {
                            if((ps_left_mb + 1)->u1_mb_type != I_4x4_MB)
                            {
                                if(u2_use_left_mb_pack >> 8)
                                {
                                    *(WORD32*)(pi1_left_pred_mode + 4) =
                                                    DC_DC_DC_DC;
                                }
                                else
                                {
                                    *(WORD32*)(pi1_left_pred_mode + 4) =
                                                    NOT_VALID;
                                }
                            }
                        }
                        else
                        {
                            *(WORD32*)(pi1_left_pred_mode + 4) = NOT_VALID;
                        }

                        pi1_left_pred_mode[1] = pi1_left_pred_mode[2];
                        pi1_left_pred_mode[2] = pi1_left_pred_mode[4];
                        pi1_left_pred_mode[3] = pi1_left_pred_mode[6];
                        *(WORD32*)(pi1_left_pred_mode + 4) =
                                        *(WORD32*)pi1_left_pred_mode;
                    }
                    else
                    {
                        pi1_left_pred_mode[7] = pi1_left_pred_mode[3];
                        pi1_left_pred_mode[6] = pi1_left_pred_mode[3];
                        pi1_left_pred_mode[5] = pi1_left_pred_mode[2];
                        pi1_left_pred_mode[4] = pi1_left_pred_mode[2];
                        pi1_left_pred_mode[3] = pi1_left_pred_mode[1];
                        pi1_left_pred_mode[2] = pi1_left_pred_mode[1];
                        pi1_left_pred_mode[1] = pi1_left_pred_mode[0];
                    }
                }
                pi1_left_pred_mode += (u1_topmb) ? 0 : 4;
            }
            else
            {
                pi1_left_pred_mode += (u1_topmb) ? 0 : 4;

                if(u1_left_available)
                {
                    if(ps_left_mb->u1_mb_type != I_4x4_MB)
                    {
                        *(WORD32*)pi1_left_pred_mode =
                                        (u2_use_left_mb_pack) ?
                                        DC_DC_DC_DC :
                                                                NOT_VALID;
                    }
                }
                else
                {
                    *(WORD32*)pi1_left_pred_mode = NOT_VALID;
                }
            }
        }

        /* One time pointer initialisations*/
        pi2_y_coeff1 = pi2_y_coeff;

        if(u1_use_top_left_mb)
        {
            pu1_top_left = pu1_ytop_left;
        }
        else
        {
            pu1_top_left = NULL;
        }

        /* Scan the sub-blocks in Raster Scan Order */
        for(u1_sub_mb_num = 0; u1_sub_mb_num < 4; u1_sub_mb_num++)
        {
            u1_sub_blk_x = (u1_sub_mb_num & 0x1);
            u1_sub_blk_y = (u1_sub_mb_num >> 1);
            i1_top_pred_mode = pi1_cur_pred_mode[u1_sub_blk_x << 1];
            i1_left_pred_mode = pi1_left_pred_mode[u1_sub_blk_y << 1];

            if(2 == u1_sub_mb_num)
            {
                i1_left_pred_mode = pi1_left_pred_mode[(u1_sub_blk_y << 1)
                                + u4_4x4_left_offset];
            }

            u1_use_top_right_mb = (!!CHECKBIT(ui2_top_rt_mask, u1_sub_mb_num));

            /*********** left subblock availability**********/
            if(u1_sub_blk_x)
            {
                u1_is_left_sub_block = 1;
            }
            else
            {
                u1_is_left_sub_block =
                                (u1_sub_blk_y < 1) ?
                                                (CHECKBIT(u2_use_left_mb_pack,
                                                          0)) :
                                                (u2_use_left_mb_pack >> 8);
            }

            /***************** Top *********************/
            if(u1_sub_blk_y)
            {
                u1_is_top_sub_block = 1;
                // sushant
                pu1_top = /*pu1_luma_pred_buffer*/pu1_luma_rec_buffer - ui_rec_width;
            }
            else
            {
                pu1_top += 8;
            }

            /***************** Left *********************/
            if((u1_sub_blk_x) | (u4_num_pmbair != 0))
            {
                // sushant
                pu1_left = /*pu1_luma_pred_buffer*/pu1_luma_rec_buffer - 1;
                ui2_left_pred_buf_width = ui_rec_width;
            }
            else
            {
                pu1_left = pu1_yleft;
                pu1_yleft += (ui_rec_width << 3);
                ui2_left_pred_buf_width = ui_rec_width;
            }

            /***************** Top Left *********************/
            if(u1_sub_mb_num)
            {
                pu1_top_left = (u1_sub_blk_x) ?
                                pu1_top - 1 : pu1_left - ui_rec_width;

                if((u1_sub_blk_x && (!u1_is_top_sub_block))
                                || ((!u1_sub_blk_x) && (!u1_is_left_sub_block)))
                {
                    pu1_top_left = NULL;
                }
            }

            /*---------------------------------------------------------------*/
            /* Calculation of Intra prediction mode                          */
            /*---------------------------------------------------------------*/
            i1_intra_pred = ((i1_left_pred_mode < 0) | (i1_top_pred_mode < 0)) ?
                            DC : MIN(i1_left_pred_mode, i1_top_pred_mode);
            {
                UWORD8 u1_packed_modes = (u1_is_top_sub_block << 1)
                                + u1_is_left_sub_block;
                UWORD8 *pu1_intra_err_codes =
                                (UWORD8 *)gau1_ih264d_intra_pred_err_code;

                /********************************************************************/
                /* Same intra4x4_pred_mode array is filled with intra4x4_pred_mode  */
                /* for a MB with 8x8 intrapredicition                               */
                /********************************************************************/
                PROFILE_DISABLE_INTRA_PRED()
                if(!pu1_prev_intra4x4_pred_mode_flag[u1_sub_mb_num])
                {
                    i1_intra_pred = pu1_rem_intra4x4_pred_mode[u1_sub_mb_num]
                                    + (pu1_rem_intra4x4_pred_mode[u1_sub_mb_num]
                                                    >= i1_intra_pred);
                }
                i1_intra_pred = CLIP3(0, 8, i1_intra_pred);
                {
                    UWORD8 u1_err_code = pu1_intra_err_codes[i1_intra_pred];

                    if((u1_err_code & u1_packed_modes) ^ u1_err_code)
                    {
                        i1_intra_pred = 0;
                        ps_dec->i4_error_code = ERROR_INTRAPRED;
                    }
                }
            }

            {
                /* Align the size to multiple of 8, so that SIMD functions
                can read 64 bits at a time. Only 25 bytes are actaully used */
                UWORD8 au1_ngbr_pels[32] = {0};
                WORD32 ngbr_avail;
                ngbr_avail = u1_is_left_sub_block << 0;
                ngbr_avail |= u1_is_top_sub_block << 2;

                if(pu1_top_left)
                    ngbr_avail |= 1 << 1;

                ngbr_avail |= u1_use_top_right_mb << 3;
                PROFILE_DISABLE_INTRA_PRED()
                {
                    ps_dec->pf_intra_pred_ref_filtering(pu1_left, pu1_top_left,
                                                        pu1_top, au1_ngbr_pels,
                                                        ui2_left_pred_buf_width,
                                                        ngbr_avail);

                    ps_dec->apf_intra_pred_luma_8x8[i1_intra_pred](
                                    au1_ngbr_pels, pu1_luma_rec_buffer, 1,
                                    ui_rec_width,
                                    ((u1_is_top_sub_block << 2) | u1_is_left_sub_block));
                }
            }

            /* Inverse Transform and Reconstruction */
            if(CHECKBIT(ps_cur_mb_info->u1_cbp, u1_sub_mb_num))
            {
                WORD16 *pi2_scale_matrix_ptr;
                WORD16 ai2_tmp[64];

                pi2_scale_matrix_ptr =
                                ps_dec->s_high_profile.i2_scalinglist8x8[0];
                PROFILE_DISABLE_IQ_IT_RECON()
                {
                    if(CHECKBIT(u4_luma_dc_only_cbp, u1_sub_mb_num))
                    {
                        ps_dec->pf_iquant_itrans_recon_luma_8x8_dc(
                                        pi2_y_coeff1,
                                        pu1_luma_rec_buffer,
                                        pu1_luma_rec_buffer,
                                        ui_rec_width,
                                        ui_rec_width,
                                        gau1_ih264d_dequant8x8_cavlc[ps_cur_mb_info->u1_qp_rem6],
                                        (UWORD16 *)pi2_scale_matrix_ptr,
                                        ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                        NULL);
                    }
                    else
                    {
                        ps_dec->pf_iquant_itrans_recon_luma_8x8(
                                        pi2_y_coeff1,
                                        pu1_luma_rec_buffer,
                                        pu1_luma_rec_buffer,
                                        ui_rec_width,
                                        ui_rec_width,
                                        gau1_ih264d_dequant8x8_cavlc[ps_cur_mb_info->u1_qp_rem6],
                                        (UWORD16 *)pi2_scale_matrix_ptr,
                                        ps_cur_mb_info->u1_qp_div6, ai2_tmp, 0,
                                        NULL);
                    }
                }

            }

            /*---------------------------------------------------------------*/
            /* Update sub block number                                       */
            /*---------------------------------------------------------------*/
            pi2_y_coeff1 += 64;

            pu1_luma_rec_buffer +=
                            (u1_sub_blk_x == 1) ?
                                            (ui_rec_width << 3) - (8 * 1) : 8;

            /*---------------------------------------------------------------*/
            /* Pred mode filled in terms of 4x4 block so replicated in 2     */
            /* locations.                                                    */
            /*---------------------------------------------------------------*/
            pi1_cur_pred_mode[u1_sub_blk_x << 1] = i1_intra_pred;
            pi1_cur_pred_mode[(u1_sub_blk_x << 1) + 1] = i1_intra_pred;
            pi1_left_pred_mode[u1_sub_blk_y << 1] = i1_intra_pred;
            pi1_left_pred_mode[(u1_sub_blk_y << 1) + 1] = i1_intra_pred;
        }
    }
    /* Decode Chroma Block */
    ih264d_unpack_chroma_coeff4x4_mb(ps_dec,
                                     ps_cur_mb_info);
    /*--------------------------------------------------------------------*/
    /* Chroma Blocks decoding                                             */
    /*--------------------------------------------------------------------*/
    {
        UWORD8 u1_intra_chrom_pred_mode;
        UWORD8 u1_chroma_cbp = (UWORD8)(ps_cur_mb_info->u1_cbp >> 4);

        /*--------------------------------------------------------------------*/
        /* Perform Chroma intra prediction                                    */
        /*--------------------------------------------------------------------*/

        u1_intra_chrom_pred_mode = CHROMA_TO_LUMA_INTRA_MODE(
                        ps_cur_mb_info->u1_chroma_pred_mode);

        {
            UWORD8 u1_packed_modes = (u1_top_available << 1)
                            + u1_left_available;
            UWORD8 u1_err_code =
                            (u1_intra_chrom_pred_mode & 1) ?
                                            u1_intra_chrom_pred_mode :
                                            (u1_intra_chrom_pred_mode ^ 2);
            if((u1_err_code & u1_packed_modes) ^ u1_err_code)
            {
                u1_intra_chrom_pred_mode = 0;
                ps_dec->i4_error_code = ERROR_INTRAPRED;
            }
        }

        /* CHANGED CODE */
        if(u1_chroma_cbp != CBPC_ALLZERO)
        {
            UWORD16 u2_chroma_csbp =
                            (u1_chroma_cbp == CBPC_ACZERO) ?
                                            0 : ps_cur_mb_info->u2_chroma_csbp;
            UWORD32 u4_scale_u;
            UWORD32 u4_scale_v;

            {
                UWORD16 au2_ngbr_pels[33];
                UWORD8 *pu1_ngbr_pels = (UWORD8 *)au2_ngbr_pels;
                UWORD16 *pu2_left_uv;
                UWORD16 *pu2_topleft_uv;
                WORD32 use_left1 = (u2_use_left_mb_pack & 0x0ff);
                WORD32 use_left2 = (u2_use_left_mb_pack & 0xff00) >> 8;

                pu2_left_uv = (UWORD16 *)pu1_uleft;
                pu2_topleft_uv = (UWORD16 *)pu1_u_top_left;
                /* Get neighbour pixels */
                /* left pels */
                if(u2_use_left_mb_pack)
                {
                    WORD32 i;
                    if(use_left1)
                    {
                        for(i = 0; i < 4; i++)
                            au2_ngbr_pels[8 - 1 - i] = pu2_left_uv[i
                                            * u4_recwidth_cr / YUV420SP_FACTOR];
                    }
                    else
                    {
                        memset(au2_ngbr_pels + 4, 0, 4 * sizeof(UWORD16));
                    }

                    if(use_left2)
                    {
                        for(i = 4; i < 8; i++)
                            au2_ngbr_pels[8 - 1 - i] = pu2_left_uv[i
                                            * u4_recwidth_cr / YUV420SP_FACTOR];
                    }
                    else
                    {
                        memset(au2_ngbr_pels, 0, 4 * sizeof(UWORD16));
                    }
                }
                else
                {
                    memset(au2_ngbr_pels, 0, 8 * sizeof(UWORD16));
                }

                /* top left pels */
                au2_ngbr_pels[8] = *pu2_topleft_uv;

                /* top pels */
                if(uc_useTopMB)
                {
                    memcpy(au2_ngbr_pels + 8 + 1, pu1_top_u,
                           8 * sizeof(UWORD16));
                }
                else
                {
                    memset(au2_ngbr_pels + 8 + 1, 0, 8 * sizeof(UWORD16));
                }

                PROFILE_DISABLE_INTRA_PRED()
                ps_dec->apf_intra_pred_chroma[u1_intra_chrom_pred_mode](
                                pu1_ngbr_pels,
                                pu1_mb_cb_rei1_buffer,
                                1,
                                u4_recwidth_cr,
                                ((uc_useTopMB << 2) | (use_left2 << 4)
                                                | use_left1));
            }
            u4_scale_u = ps_cur_mb_info->u1_qpc_div6;
            u4_scale_v = ps_cur_mb_info->u1_qpcr_div6;
            pi2_y_coeff = ps_dec->pi2_coeff_data;

            {
                UWORD32 i;
                WORD16 ai2_tmp[16];
                for(i = 0; i < 4; i++)
                {
                    WORD16 *pi2_level = pi2_y_coeff + (i << 4);
                    UWORD8 *pu1_pred_sblk = pu1_mb_cb_rei1_buffer
                                    + ((i & 0x1) * BLK_SIZE * YUV420SP_FACTOR)
                                    + (i >> 1) * (u4_recwidth_cr << 2);
                    PROFILE_DISABLE_IQ_IT_RECON()
                    {
                        if(CHECKBIT(u2_chroma_csbp, i))
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpc_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[1],
                                            u4_scale_u, ai2_tmp, pi2_level);
                        }
                        else if(pi2_level[0] != 0)
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4_dc(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpc_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[1],
                                            u4_scale_u, ai2_tmp, pi2_level);
                        }
                    }

                }
            }

            pi2_y_coeff += MB_CHROM_SIZE;
            u2_chroma_csbp = u2_chroma_csbp >> 4;
            {
                UWORD32 i;
                WORD16 ai2_tmp[16];
                for(i = 0; i < 4; i++)
                {
                    WORD16 *pi2_level = pi2_y_coeff + (i << 4);
                    UWORD8 *pu1_pred_sblk = pu1_mb_cb_rei1_buffer + 1
                                    + ((i & 0x1) * BLK_SIZE * YUV420SP_FACTOR)
                                    + (i >> 1) * (u4_recwidth_cr << 2);
                    PROFILE_DISABLE_IQ_IT_RECON()
                    {
                        if(CHECKBIT(u2_chroma_csbp, i))
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpcr_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[2],
                                            u4_scale_v, ai2_tmp, pi2_level);
                        }
                        else if(pi2_level[0] != 0)
                        {
                            ps_dec->pf_iquant_itrans_recon_chroma_4x4_dc(
                                            pi2_level,
                                            pu1_pred_sblk,
                                            pu1_pred_sblk,
                                            u4_recwidth_cr,
                                            u4_recwidth_cr,
                                            gau2_ih264_iquant_scale_4x4[ps_cur_mb_info->u1_qpcr_rem6],
                                            (UWORD16 *)ps_dec->s_high_profile.i2_scalinglist4x4[2],
                                            u4_scale_v, ai2_tmp, pi2_level);
                        }
                    }
                }
            }

        }
        else
        {
            /* If no inverse transform is needed, pass recon buffer pointer */
            /* to Intraprediction module instead of pred buffer pointer     */
            {
                UWORD16 au2_ngbr_pels[33];
                UWORD8 *pu1_ngbr_pels = (UWORD8 *)au2_ngbr_pels;
                UWORD16 *pu2_left_uv;
                UWORD16 *pu2_topleft_uv;
                WORD32 use_left1 = (u2_use_left_mb_pack & 0x0ff);
                WORD32 use_left2 = (u2_use_left_mb_pack & 0xff00) >> 8;

                pu2_topleft_uv = (UWORD16 *)pu1_u_top_left;
                pu2_left_uv = (UWORD16 *)pu1_uleft;

                /* Get neighbour pixels */
                /* left pels */
                if(u2_use_left_mb_pack)
                {
                    WORD32 i;
                    if(use_left1)
                    {
                        for(i = 0; i < 4; i++)
                            au2_ngbr_pels[8 - 1 - i] = pu2_left_uv[i
                                            * u4_recwidth_cr / YUV420SP_FACTOR];
                    }
                    else
                    {
                        memset(au2_ngbr_pels + 4, 0, 4 * sizeof(UWORD16));
                    }

                    if(use_left2)
                    {
                        for(i = 4; i < 8; i++)
                            au2_ngbr_pels[8 - 1 - i] = pu2_left_uv[i
                                            * u4_recwidth_cr / YUV420SP_FACTOR];
                    }
                    else
                    {
                        memset(au2_ngbr_pels, 0, 4 * sizeof(UWORD16));
                    }

                }
                else
                {
                    memset(au2_ngbr_pels, 0, 8 * sizeof(UWORD16));
                }

                /* top left pels */
                au2_ngbr_pels[8] = *pu2_topleft_uv;

                /* top pels */
                if(uc_useTopMB)
                {
                    memcpy(au2_ngbr_pels + 8 + 1, pu1_top_u,
                           8 * sizeof(UWORD16));
                }
                else
                {
                    memset(au2_ngbr_pels + 8 + 1, 0, 8 * sizeof(UWORD16));
                }

                PROFILE_DISABLE_INTRA_PRED()
                ps_dec->apf_intra_pred_chroma[u1_intra_chrom_pred_mode](
                                pu1_ngbr_pels,
                                pu1_mb_cb_rei1_buffer,
                                1,
                                u4_recwidth_cr,
                                ((uc_useTopMB << 2) | (use_left2 << 4)
                                                | use_left1));
            }

        }

    }
    return OK;
}
