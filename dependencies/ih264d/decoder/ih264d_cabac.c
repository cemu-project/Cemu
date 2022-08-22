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
 * \file ih264d_cabac.c
 *
 * \brief
 *    This file contains Binary decoding routines.
 *
 * \date
 *    04/02/2003
 *
 * \author  NS
 ***************************************************************************
 */
#include <string.h>
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"
#include "ih264d_cabac.h"
#include "ih264d_bitstrm.h"
#include "ih264d_error_handler.h"
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_parse_cabac.h"
#include "ih264d_tables.h"



/*!
 **************************************************************************
 * \if Function name : ih264d_init_cabac_dec_envirnoment \endif
 *
 * \brief
 *    This function initializes CABAC decoding envirnoment. This function
 *    implements 9.3.3.2.3.1 of ISO/IEC14496-10.
 *
 * \return
 *    None
 *
 **************************************************************************
 */
WORD32 ih264d_init_cabac_dec_envirnoment(decoding_envirnoment_t * ps_cab_env,
                                       dec_bit_stream_t *ps_bitstrm)
{
    UWORD32 u4_code_int_val_ofst;

    ps_cab_env->u4_code_int_range = (HALF - 2) << 23;
    NEXTBITS(u4_code_int_val_ofst, ps_bitstrm->u4_ofst, ps_bitstrm->pu4_buffer,
             32);
    FLUSHBITS(ps_bitstrm->u4_ofst, 9)

    if(EXCEED_OFFSET(ps_bitstrm))
        return ERROR_EOB_FLUSHBITS_T;

    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;

    /*brief description of the design adopted for CABAC*/
    /*according to the standard the u4_code_int_range needs to be initialized 0x 1FE(10 bits) and
     9 bits from the bit stream need to be read and into the u4_code_int_val_ofst.As and when the
     u4_code_int_range becomes less than 10 bits we need to renormalize and read from the bitstream*

     In the implemented design
     initially

     range_new = range <<23
     valOffset_new = valOffset << 23 + 23 bits(read from the bit stream)

     Thus we have read 23 more bits ahead of time.

     It can be mathematical proved that even with the modified range and u4_ofst the operations
     like comparison and subtraction needed for a bin decode are still valid(both in the regular case and the bypass case)

     As bins are decoded..we consume the bits that we have already read into the valOffset.The clz of Range
     gives us the number of bits we consumed of the 23 bits that we have read ahead of time.

     when the number bits we have consumed exceeds 23 ,we renormalize..and  we read from the bitstream again*/

RESET_BIN_COUNTS(ps_cab_env)

    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_init_cabac_contexts                                      */
/*                                                                           */
/*  Description   : This function initializes the cabac contexts             */
/*                  depending upon slice type and Init_Idc value.            */
/*  Inputs        : ps_dec, slice type                                       */
/*  Globals       : <Does it use any global variables?>                      */
/*  Outputs       :                                                          */
/*  Returns       : void                                                     */
/*                                                                           */
/*  Issues        : none                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         03 05 2005   100153)         Draft                                */
/*                                                                           */
/*****************************************************************************/

void ih264d_init_cabac_contexts(UWORD8 u1_slice_type, dec_struct_t * ps_dec)
{

    bin_ctxt_model_t *p_cabac_ctxt_table_t = ps_dec->p_cabac_ctxt_table_t;
    UWORD8 u1_qp_y = ps_dec->ps_cur_slice->u1_slice_qp;
    UWORD8 u1_cabac_init_Idc = 0;

    if(I_SLICE != u1_slice_type)
    {
        u1_cabac_init_Idc = ps_dec->ps_cur_slice->u1_cabac_init_idc;
    }

    {
        /* MAKING ps_dec->p_ctxt_inc_mb_map a scratch buffer */
        /* 0th entry of CtxtIncMbMap will be always be containing default values
         for CABAC context representing MB not available */
        ctxt_inc_mb_info_t *p_DefCtxt = ps_dec->p_ctxt_inc_mb_map - 1;
        UWORD8 *pu1_temp;
        WORD8 i;
        p_DefCtxt->u1_mb_type = CAB_SKIP;

        p_DefCtxt->u1_cbp = 0x0f;
        p_DefCtxt->u1_intra_chroma_pred_mode = 0;

        p_DefCtxt->u1_yuv_dc_csbp = 0x7;

        p_DefCtxt->u1_transform8x8_ctxt = 0;

        pu1_temp = (UWORD8*)p_DefCtxt->i1_ref_idx;
        for(i = 0; i < 4; i++, pu1_temp++)
            (*pu1_temp) = 0;
        pu1_temp = (UWORD8*)p_DefCtxt->u1_mv;
        for(i = 0; i < 16; i++, pu1_temp++)
            (*pu1_temp) = 0;
        ps_dec->ps_def_ctxt_mb_info = p_DefCtxt;
    }

    if(u1_slice_type == I_SLICE)
    {
        u1_cabac_init_Idc = 3;
        ps_dec->p_mb_type_t = p_cabac_ctxt_table_t + MB_TYPE_I_SLICE;
    }
    else if(u1_slice_type == P_SLICE)
    {
        ps_dec->p_mb_type_t = p_cabac_ctxt_table_t + MB_TYPE_P_SLICE;
        ps_dec->p_mb_skip_flag_t = p_cabac_ctxt_table_t + MB_SKIP_FLAG_P_SLICE;
        ps_dec->p_sub_mb_type_t = p_cabac_ctxt_table_t + SUB_MB_TYPE_P_SLICE;
    }
    else if(u1_slice_type == B_SLICE)
    {
        ps_dec->p_mb_type_t = p_cabac_ctxt_table_t + MB_TYPE_B_SLICE;
        ps_dec->p_mb_skip_flag_t = p_cabac_ctxt_table_t + MB_SKIP_FLAG_B_SLICE;
        ps_dec->p_sub_mb_type_t = p_cabac_ctxt_table_t + SUB_MB_TYPE_B_SLICE;
    }
    {
        bin_ctxt_model_t *p_cabac_ctxt_table_t_tmp = p_cabac_ctxt_table_t;
        if(ps_dec->ps_cur_slice->u1_field_pic_flag)
        {
            p_cabac_ctxt_table_t_tmp += SIGNIFICANT_COEFF_FLAG_FLD;

        }
        else
        {
            p_cabac_ctxt_table_t_tmp += SIGNIFICANT_COEFF_FLAG_FRAME;
        }
        {
            bin_ctxt_model_t * * p_significant_coeff_flag_t =
                            ps_dec->p_significant_coeff_flag_t;
            p_significant_coeff_flag_t[0] = p_cabac_ctxt_table_t_tmp
                            + SIG_COEFF_CTXT_CAT_0_OFFSET;
            p_significant_coeff_flag_t[1] = p_cabac_ctxt_table_t_tmp
                            + SIG_COEFF_CTXT_CAT_1_OFFSET;
            p_significant_coeff_flag_t[2] = p_cabac_ctxt_table_t_tmp
                            + SIG_COEFF_CTXT_CAT_2_OFFSET;
            p_significant_coeff_flag_t[3] = p_cabac_ctxt_table_t_tmp
                            + SIG_COEFF_CTXT_CAT_3_OFFSET;
            p_significant_coeff_flag_t[4] = p_cabac_ctxt_table_t_tmp
                            + SIG_COEFF_CTXT_CAT_4_OFFSET;

            p_significant_coeff_flag_t[5] = p_cabac_ctxt_table_t_tmp
                            + SIG_COEFF_CTXT_CAT_5_OFFSET;

        }
    }

    memcpy(p_cabac_ctxt_table_t,
           gau1_ih264d_cabac_ctxt_init_table[u1_cabac_init_Idc][u1_qp_y],
           NUM_CABAC_CTXTS * sizeof(bin_ctxt_model_t));
}
/*!
 **************************************************************************
 * \if Function name : ih264d_decode_bin \endif
 *
 * \brief
 *    This function implements decoding process of a decision as defined
 *    in 9.3.3.2.2.
 *
 * \return
 *    Returns symbol decoded.
 *
 * \note
 *    It is specified in 9.3.3.2.3.2 that, one of the input to this function
 *    is CtxIdx. CtxIdx is used to identify state and MPS of that context
 *    (Refer Fig 9.11 - Flowchart for encoding a decision). To suffice that
 *    here we pass a pointer bin_ctxt_model_t which contains these values.
 *
 **************************************************************************
 */

UWORD32 ih264d_decode_bin(UWORD32 u4_ctx_inc,
                          bin_ctxt_model_t *ps_src_bin_ctxt,
                          dec_bit_stream_t *ps_bitstrm,
                          decoding_envirnoment_t *ps_cab_env)

{

    UWORD32 u4_qnt_int_range, u4_code_int_range, u4_code_int_val_ofst,
                    u4_int_range_lps;

    UWORD32 u4_symbol, u4_mps_state;

    bin_ctxt_model_t *ps_bin_ctxt;

    UWORD32 table_lookup;
    const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;
    UWORD32 u4_clz;

    ps_bin_ctxt = ps_src_bin_ctxt + u4_ctx_inc;

    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    u4_mps_state = (ps_bin_ctxt->u1_mps_state);
    u4_clz = CLZ(u4_code_int_range);

    u4_qnt_int_range = u4_code_int_range << u4_clz;
    u4_qnt_int_range = (u4_qnt_int_range >> 29) & 0x3;

    table_lookup = pu4_table[(u4_mps_state << 2) + u4_qnt_int_range];
    u4_int_range_lps = table_lookup & 0xff;

    u4_int_range_lps = u4_int_range_lps << (23 - u4_clz);
    u4_code_int_range = u4_code_int_range - u4_int_range_lps;

    u4_symbol = ((u4_mps_state >> 6) & 0x1);

    u4_mps_state = (table_lookup >> 8) & 0x7F;

    CHECK_IF_LPS(u4_code_int_range, u4_code_int_val_ofst, u4_symbol,
                 u4_int_range_lps, u4_mps_state, table_lookup)

    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_8)
    {
        UWORD32 *pu4_buffer, u4_offset;

        pu4_buffer = ps_bitstrm->pu4_buffer;
        u4_offset = ps_bitstrm->u4_ofst;

        RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst, u4_offset,
                            pu4_buffer)

        ps_bitstrm->u4_ofst = u4_offset;
    }

    INC_BIN_COUNT(ps_cab_env)

    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;
    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_bin_ctxt->u1_mps_state = u4_mps_state;

    return (u4_symbol);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_decode_terminate \endif
 *
 * \brief
 *    This function implements decoding process of a termination as defined
 *    9.3.3.2.2.3 of ISO/IEC14496-10.
 *
 * \return
 *    Returns symbol decoded.
 *
 * \note
 *    This routine is called while decoding "end_of_skice_flag" and of the
 *    bin indicating PCM mode in MBType.
 *
 **************************************************************************
 */
UWORD8 ih264d_decode_terminate(decoding_envirnoment_t * ps_cab_env,
                               dec_bit_stream_t * ps_stream)
{
    UWORD32 u4_symbol;
    UWORD32 u4_code_int_val_ofst, u4_code_int_range;
    UWORD32 u4_clz;

    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    u4_clz = CLZ(u4_code_int_range);
    u4_code_int_range -= (2 << (23 - u4_clz));

    if(u4_code_int_val_ofst >= u4_code_int_range)
    {
        /* S=1 */
        u4_symbol = 1;

        {

            /*the u4_ofst needs to be updated before termination*/
            ps_stream->u4_ofst += u4_clz;

        }

    }
    else
    {
        /* S=0 */
        u4_symbol = 0;

        if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_8)
        {
            UWORD32 *pu4_buffer, u4_offset;

            pu4_buffer = ps_stream->pu4_buffer;
            u4_offset = ps_stream->u4_ofst;

            RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst, u4_offset,
                                pu4_buffer)
            ps_stream->u4_ofst = u4_offset;
        }
    }

    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;

    INC_BIN_COUNT(ps_cab_env)

    return (u4_symbol);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_decode_bins_tunary                                */
/*                                                                           */
/*  Description   : This function decodes bins in the case of TUNARY         */
/*                  binarization technique.valid_length is assumed  equal to 3 */
/*                  and u1_max_bins <= 4 in this functon.                                              */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : <Describe how the function operates - include algorithm  */
/*                  description>                                             */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        :                                                          */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         20 11 2008   SH          Draft                                   */
/*                                                                           */
/*****************************************************************************/

UWORD32 ih264d_decode_bins_tunary(UWORD8 u1_max_bins,
                                  UWORD32 u4_ctx_inc,
                                  bin_ctxt_model_t *ps_src_bin_ctxt,
                                  dec_bit_stream_t *ps_bitstrm,
                                  decoding_envirnoment_t *ps_cab_env)

{
    UWORD32 u4_value;
    UWORD32 u4_symbol;
    UWORD8 u4_ctx_Inc;
    bin_ctxt_model_t *ps_bin_ctxt;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;
    const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;

    u4_value = 0;

    /*u1_max_bins has to be less than or equal to 4, u1_max_bins <= 4 for  this function*/

    /*here the valid length is assumed to be equal to 3 ,so the calling function is expected
     to duplicate CtxInc if valid lenth is 2 and cmaxbin is greater than2*/
    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    do
    {
        u4_ctx_Inc = u4_ctx_inc & 0xF;
        u4_ctx_inc = u4_ctx_inc >> 4;

        ps_bin_ctxt = ps_src_bin_ctxt + u4_ctx_Inc;

        DECODE_ONE_BIN_MACRO(ps_bin_ctxt, u4_code_int_range, u4_code_int_val_ofst,
                             pu4_table, ps_bitstrm, u4_symbol)

        INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);

        u4_value++;
    }
    while((u4_value < u1_max_bins) & (u4_symbol));

    u4_value = u4_value - 1 + u4_symbol;

    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;

    return (u4_value);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_decode_bins                                */
/*                                                                           */
/*  Description   : This function decodes bins in the case of MSB_FIRST_FLC  */
/*                  binarization technique.valid_length is always equal max_bins */
/*                  for MSB_FIRST_FLC. assumes  u1_max_bins <= 4               */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : <Describe how the function operates - include algorithm  */
/*                  description>                                             */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         20 11 2008   SH          Draft                                   */
/*                                                                           */
/*****************************************************************************/

UWORD32 ih264d_decode_bins(UWORD8 u1_max_bins,
                           UWORD32 u4_ctx_inc,
                           bin_ctxt_model_t *ps_src_bin_ctxt,
                           dec_bit_stream_t *ps_bitstrm,
                           decoding_envirnoment_t *ps_cab_env)

{
    UWORD32 u4_value;
    UWORD32 u4_symbol, i;
    UWORD32 u4_ctxt_inc;
    bin_ctxt_model_t *ps_bin_ctxt;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;
    const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;

    i = 0;

    u4_value = 0;

    /*u1_max_bins has to be less than or equal to 4, u1_max_bins <= 4 for  this fucntion*/
    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    do
    {
        u4_ctxt_inc = u4_ctx_inc & 0xf;
        u4_ctx_inc = u4_ctx_inc >> 4;

        ps_bin_ctxt = ps_src_bin_ctxt + u4_ctxt_inc;

        DECODE_ONE_BIN_MACRO(ps_bin_ctxt, u4_code_int_range, u4_code_int_val_ofst,
                             pu4_table, ps_bitstrm, u4_symbol)

        INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);

        u4_value = (u4_value << 1) | (u4_symbol);

        i++;
    }
    while(i < u1_max_bins);

    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;

    return (u4_value);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_decode_bins_unary                                */
/*                                                                           */
/*  Description   : This function decodes bins in the case of UNARY         */
/*                  binarization technique.here the valid length is taken to 5*/
/*                  and cmax is always greater than 9                       */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : <Describe how the function operates - include algorithm  */
/*                  description>                                             */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         20 11 2008   SH          Draft                                   */
/*                                                                           */
/*****************************************************************************/
UWORD32 ih264d_decode_bins_unary(UWORD8 u1_max_bins,
                                 UWORD32 u4_ctx_inc,
                                 bin_ctxt_model_t *ps_src_bin_ctxt,
                                 dec_bit_stream_t *ps_bitstrm,
                                 decoding_envirnoment_t *ps_cab_env)
{
    UWORD32 u4_value;
    UWORD32 u4_symbol;
    bin_ctxt_model_t *ps_bin_ctxt;
    UWORD32 u4_ctx_Inc;
    UWORD32 u4_code_int_range, u4_code_int_val_ofst;
    const UWORD32 *pu4_table = (const UWORD32 *)ps_cab_env->cabac_table;

    /* in this function the valid length for u4_ctx_inc is always taken to be,so if the
     the valid length is lessthan 5 the caller need to duplicate accordingly*/

    /*u1_max_bins is always greater or equal to 9 we have the check for u1_max_bins only after the 2 loop*/
    u4_value = 0;
    u4_code_int_range = ps_cab_env->u4_code_int_range;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;

    do
    {
        u4_ctx_Inc = u4_ctx_inc & 0xf;
        u4_ctx_inc = u4_ctx_inc >> 4;

        ps_bin_ctxt = ps_src_bin_ctxt + u4_ctx_Inc;

        DECODE_ONE_BIN_MACRO(ps_bin_ctxt, u4_code_int_range, u4_code_int_val_ofst,
                             pu4_table, ps_bitstrm, u4_symbol)

        INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);

        u4_value++;

    }
    while(u4_symbol && u4_value < 4);

    if(u4_symbol && (u4_value < u1_max_bins))
    {

        u4_ctx_Inc = u4_ctx_inc & 0xf;

        ps_bin_ctxt = ps_src_bin_ctxt + u4_ctx_Inc;

        do
        {

            DECODE_ONE_BIN_MACRO(ps_bin_ctxt, u4_code_int_range, u4_code_int_val_ofst,
                                 pu4_table, ps_bitstrm, u4_symbol)

            INC_BIN_COUNT(ps_cab_env);INC_DECISION_BINS(ps_cab_env);

            u4_value++;

        }
        while(u4_symbol && (u4_value < u1_max_bins));

    }

    ps_cab_env->u4_code_int_range = u4_code_int_range;
    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;

    u4_value = u4_value - 1 + u4_symbol;

    return (u4_value);

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_decode_bypass_bins_unary                                     */
/*                                                                           */
/*  Description   : This function is used in the case of UNARY coding       */
/*                                                                           */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : <Describe how the function operates - include algorithm  */
/*                  description>                                             */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 10 2005   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 ih264d_decode_bypass_bins_unary(decoding_envirnoment_t *ps_cab_env,
                                        dec_bit_stream_t *ps_bitstrm)
{
    UWORD32 u4_value;
    UWORD32 u4_bin;
    UWORD32 u4_code_int_val_ofst, u4_code_int_range;

    UWORD32 u1_max_bins;

    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;
    u4_code_int_range = ps_cab_env->u4_code_int_range;

    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_9)
    {
        UWORD32 *pu4_buffer, u4_offset;

        pu4_buffer = ps_bitstrm->pu4_buffer;
        u4_offset = ps_bitstrm->u4_ofst;

        RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst, u4_offset,
                            pu4_buffer)
        ps_bitstrm->u4_ofst = u4_offset;
    }

    /*as it is called only form mvd*/
    u1_max_bins = 32;
    u4_value = 0;

    do
    {
        u4_value++;

        u4_code_int_range = u4_code_int_range >> 1;
        if(u4_code_int_val_ofst >= u4_code_int_range)
        {
            /* S=1 */
            u4_bin = 1;
            u4_code_int_val_ofst -= u4_code_int_range;
        }
        else
        {
            /* S=0 */
            u4_bin = 0;
        }

        INC_BIN_COUNT(ps_cab_env);INC_BYPASS_BINS(ps_cab_env);

        if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_9)
        {
            UWORD32 *pu4_buffer, u4_offset;

            pu4_buffer = ps_bitstrm->pu4_buffer;
            u4_offset = ps_bitstrm->u4_ofst;

            RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst, u4_offset,
                                pu4_buffer)

            ps_bitstrm->u4_ofst = u4_offset;
        }

    }
    while(u4_bin && (u4_value < u1_max_bins));

    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;
    ps_cab_env->u4_code_int_range = u4_code_int_range;
    u4_value = (u4_value - 1 + u4_bin);

return (u4_value);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_decode_bypass_bins                                     */
/*                                                                           */
/*  Description   : This function is used in the case of FLC coding       */
/*                                                                           */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : <Describe how the function operates - include algorithm  */
/*                  description>                                             */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         13 10 2005   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

UWORD32 ih264d_decode_bypass_bins(decoding_envirnoment_t *ps_cab_env,
                                  UWORD8 u1_max_bins,
                                  dec_bit_stream_t *ps_bitstrm)
{
    UWORD32 u4_bins;
    UWORD32 u4_bin;
    UWORD32 u4_code_int_val_ofst, u4_code_int_range;

    u4_bins = 0;
    u4_code_int_val_ofst = ps_cab_env->u4_code_int_val_ofst;
    u4_code_int_range = ps_cab_env->u4_code_int_range;

    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_9)
    {
        UWORD32 *pu4_buffer, u4_offset;

        pu4_buffer = ps_bitstrm->pu4_buffer;
        u4_offset = ps_bitstrm->u4_ofst;

        RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst, u4_offset,
                            pu4_buffer)
        ps_bitstrm->u4_ofst = u4_offset;
    }

    do
    {

        u4_code_int_range = u4_code_int_range >> 1;

        if(u4_code_int_val_ofst >= u4_code_int_range)
        {
            /* S=1 */
            u4_bin = 1;
            u4_code_int_val_ofst -= u4_code_int_range;
        }
        else
        {
            /* S=0 */
            u4_bin = 0;
        }

        INC_BIN_COUNT(ps_cab_env);INC_BYPASS_BINS(ps_cab_env);

        u4_bins = ((u4_bins << 1) | u4_bin);
        u1_max_bins--;

        if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_9)
        {
            UWORD32 *pu4_buffer, u4_offset;

            pu4_buffer = ps_bitstrm->pu4_buffer;
            u4_offset = ps_bitstrm->u4_ofst;

            RENORM_RANGE_OFFSET(u4_code_int_range, u4_code_int_val_ofst, u4_offset,
                                pu4_buffer)
            ps_bitstrm->u4_ofst = u4_offset;
        }

    }
    while(u1_max_bins);

    ps_cab_env->u4_code_int_val_ofst = u4_code_int_val_ofst;
    ps_cab_env->u4_code_int_range = u4_code_int_range;

    return (u4_bins);
}

