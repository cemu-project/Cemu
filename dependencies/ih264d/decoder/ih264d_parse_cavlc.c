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
 * \file ih264d_parse_cavlc.c
 *
 * \brief
 *    This file contains UVLC related functions.
 *
 * \date
 *    20/11/2002
 *
 * \author  NS
 ***************************************************************************
 */

#include <string.h>
#include <stdio.h>

#include "ih264d_bitstrm.h"
#include "ih264d_parse_cavlc.h"
#include "ih264d_error_handler.h"
#include "ih264d_defs.h"
#include "ih264d_debug.h"
#include "ih264d_cabac.h"
#include "ih264d_structs.h"
#include "ih264d_tables.h"
#include "ih264d_tables.h"
#include "ih264d_mb_utils.h"

void ih264d_unpack_coeff4x4_dc_4x4blk(tu_sblk4x4_coeff_data_t *ps_tu_4x4,
                                      WORD16 *pi2_out_coeff_data,
                                      UWORD8 *pu1_inv_scan);

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_uev                                                  */
/*                                                                           */
/*  Description   : Reads the unsigned Exp Golomb codec syntax from the      */
/*                  ps_bitstrm as specified in section 9.1 of H264 standard      */
/*                  It also increases bitstream u4_ofst by the number of bits */
/*                  parsed for UEV decode operation                          */
/*                                                                           */
/*  Inputs        : bitstream base pointer and bitsream u4_ofst in bits       */
/*  Globals       : None                                                     */
/*  Processing    :                                                          */
/*  Outputs       : UEV decoded syntax element and incremented ps_bitstrm u4_ofst */
/*  Returns       : UEV decoded syntax element                               */
/*                                                                           */
/*  Issues        : Does not check if ps_bitstrm u4_ofst exceeds max ps_bitstrm i4_size  */
/*                  for performamce. Caller might have to do error resilence */
/*                  check for bitstream overflow                             */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         19 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
UWORD32 ih264d_uev(UWORD32 *pu4_bitstrm_ofst, UWORD32 *pu4_bitstrm_buf)
{
    UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
    UWORD32 u4_word, u4_ldz;

    /***************************************************************/
    /* Find leading zeros in next 32 bits                          */
    /***************************************************************/
    NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
    u4_ldz = CLZ(u4_word);
    /* Flush the ps_bitstrm */
    u4_bitstream_offset += (u4_ldz + 1);
    /* Read the suffix from the ps_bitstrm */
    u4_word = 0;
    if(u4_ldz)
        GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf, u4_ldz);
    *pu4_bitstrm_ofst = u4_bitstream_offset;
    return ((1 << u4_ldz) + u4_word - 1);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_sev                                                  */
/*                                                                           */
/*  Description   : Reads the signed Exp Golomb codec syntax from the ps_bitstrm */
/*                  as specified in section 9.1 of H264 standard.            */
/*                  It also increases bitstream u4_ofst by the number of bits */
/*                  parsed for SEV decode operation                          */
/*                                                                           */
/*  Inputs        : bitstream base pointer and bitsream u4_ofst in bits       */
/*  Globals       : None                                                     */
/*  Processing    :                                                          */
/*  Outputs       : SEV decoded syntax element and incremented ps_bitstrm u4_ofst */
/*  Returns       : SEV decoded syntax element                               */
/*                                                                           */
/*  Issues        : Does not check if ps_bitstrm u4_ofst exceeds max ps_bitstrm i4_size  */
/*                  for performamce. Caller might have to do error resilence */
/*                  check for bitstream overflow                             */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         19 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_sev(UWORD32 *pu4_bitstrm_ofst, UWORD32 *pu4_bitstrm_buf)
{
    UWORD32 u4_bitstream_offset = *pu4_bitstrm_ofst;
    UWORD32 u4_word, u4_ldz, u4_abs_val;

    /***************************************************************/
    /* Find leading zeros in next 32 bits                          */
    /***************************************************************/
    NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
    u4_ldz = CLZ(u4_word);

    /* Flush the ps_bitstrm */
    u4_bitstream_offset += (u4_ldz + 1);

    /* Read the suffix from the ps_bitstrm */
    u4_word = 0;
    if(u4_ldz)
        GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf, u4_ldz);

    *pu4_bitstrm_ofst = u4_bitstream_offset;
    u4_abs_val = ((1 << u4_ldz) + u4_word) >> 1;

    if(u4_word & 0x1)
        return (-(WORD32)u4_abs_val);
    else
        return (u4_abs_val);
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : get_tev_range_1                                          */
/*                                                                           */
/*  Description   : Reads the TEV Exp Golomb codec syntax from the ps_bitstrm    */
/*                  as specified in section 9.1 of H264 standard. This will  */
/*                  called only when the input range is 1 for TEV decode.    */
/*                  If range is more than 1, then UEV decode is done         */
/*                                                                           */
/*  Inputs        : bitstream base pointer and bitsream u4_ofst in bits       */
/*  Globals       : None                                                     */
/*  Processing    :                                                          */
/*  Outputs       : TEV decoded syntax element and incremented ps_bitstrm u4_ofst */
/*  Returns       : TEV decoded syntax element                               */
/*                                                                           */
/*  Issues        : Does not check if ps_bitstrm u4_ofst exceeds max ps_bitstrm i4_size  */
/*                  for performamce. Caller might have to do error resilence */
/*                  check for bitstream overflow                             */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         19 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
UWORD32 ih264d_tev_range1(UWORD32 *pu4_bitstrm_ofst, UWORD32 *pu4_bitstrm_buf)
{
    UWORD32 u4_code;
    GETBIT(u4_code, *pu4_bitstrm_ofst, pu4_bitstrm_buf);
    return (!u4_code);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_uvlc \endif
 *
 * \brief
 *
 *    Reads the unsigned/signed/truncated integer Exp-Golomb-coded syntax element
 *    with the left bit first. The parsing process for this descriptor is specified
 *    in subclause 9.1.
 *
 * \param ps_bitstrm       : Pointer to Bitstream Structure .
 * \param u4_range           : Range value in case of Truncated Exp-Golomb-code
 * \param pi_bitstrm_ofst : Pointer to the local copy of Bitstream u4_ofst
 * \param u1_flag            : Flag indicating the case of UEV,SEV or TEV
 * \param u4_bitstrm_ofst : Local copy of Bitstream u4_ofst
 * \param pu4_bitstrm_buf : Pointer to the Bitstream buffer
 *
 * \return
 *    Returns Code Value.
 *
 **************************************************************************
 */

WORD32 ih264d_uvlc(dec_bit_stream_t *ps_bitstrm,
                   UWORD32 u4_range,
                   UWORD32 *pi_bitstrm_ofst,
                   UWORD8 u1_flag,
                   UWORD32 u4_bitstrm_ofst,
                   UWORD32 *pu4_bitstrm_buf)
{
    UWORD32 word, word2, cur_bit, cur_word, code_val, code_num, clz;

    SWITCHOFFTRACE;
    cur_bit = u4_bitstrm_ofst & 0x1F;
    cur_word = u4_bitstrm_ofst >> 5;
    word = pu4_bitstrm_buf[cur_word];
    word2 = pu4_bitstrm_buf[cur_word + 1];

    if(cur_bit != 0)
    {
        word <<= cur_bit;
        word2 >>= (32 - cur_bit);
        word |= word2;
    }

    if(u1_flag == TEV && u4_range == 1)
    {
        word >>= 31;
        word = 1 - word;
        (*pi_bitstrm_ofst)++;
        ps_bitstrm->u4_ofst = *pi_bitstrm_ofst;
        return (WORD32)word;
    }

    //finding clz
    {
        UWORD32 ui32_code, ui32_mask;

        ui32_code = word;
        ui32_mask = 0x80000000;
        clz = 0;

        /* DSP implements this with LMBD instruction */
        /* so there we don't need to break the loop */
        while(!(ui32_code & ui32_mask))
        {
            clz++;
            ui32_mask >>= 1;
            if(0 == ui32_mask)
                break;
        }
    }

    if(clz == 0)
    {
        *pi_bitstrm_ofst = *pi_bitstrm_ofst + (2 * clz) + 1;
        ps_bitstrm->u4_ofst = *pi_bitstrm_ofst;
        return 0;
    }

    word <<= (clz + 1);
    word >>= (32 - clz);
    code_num = (1 << clz) + word - 1;
    *pi_bitstrm_ofst = *pi_bitstrm_ofst + (2 * clz) + 1;
    ps_bitstrm->u4_ofst = *pi_bitstrm_ofst;

    if(u1_flag == TEV || u1_flag == UEV)
        return (WORD32)code_num;

    code_val = (code_num + 1) >> 1;
    if(!(code_num & 0x01))
        return -((WORD32)code_val);
    return (WORD32)code_val;

}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cavlc_4x4res_block_totalcoeff_1                          */
/*                                                                           */
/*  Description   : This function does cavlc decoding of 4x4 block residual  */
/*                  coefficient when total coeff is equal to 1. The parsing  */
/*                  is done as defined in section 9.2.2 and 9.2.3 of the     */
/*                  H264 standard.                                           */
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
/*         25 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_cavlc_4x4res_block_totalcoeff_1(UWORD32 u4_isdc,
                                           UWORD32 u4_total_coeff_trail_one,
                                           dec_bit_stream_t *ps_bitstrm)
{

    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bitstream_offset = ps_bitstrm->u4_ofst;
    UWORD32 u4_trailing_ones = u4_total_coeff_trail_one & 0xFFFF;
    WORD32 i2_level;
    UWORD32 u4_tot_zero, u4_ldz, u4_scan_pos;

    tu_sblk4x4_coeff_data_t *ps_tu_4x4;
    WORD16 *pi2_coeff_data;
    dec_struct_t *ps_dec = (dec_struct_t *)ps_bitstrm->pv_codec_handle;

    ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;
    ps_tu_4x4->u2_sig_coeff_map = 0;
    pi2_coeff_data = &ps_tu_4x4->ai2_level[0];


    if(u4_trailing_ones)
    {
        UWORD32 u4_sign;
        /****************************************************************/
        /* Decode Trailing One as in section 9.2.2                      */
        /****************************************************************/
        GETBIT(u4_sign, u4_bitstream_offset, pu4_bitstrm_buf);
        i2_level = u4_sign ? -1 : 1;
    }
    else
    {
        /****************************************************************/
        /* Decoding Level based on prefix and suffix  as in 9.2.2       */
        /****************************************************************/
        UWORD32 u4_lev_suffix, u4_lev_suffix_size;
        WORD32 u2_lev_code, u2_abs_value;
        UWORD32 u4_lev_prefix;
        /***************************************************************/
        /* Find leading zeros in next 32 bits                          */
        /***************************************************************/
        FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                              pu4_bitstrm_buf);
        u2_lev_code = (2 + MIN(u4_lev_prefix, 15));

        if(14 == u4_lev_prefix)
            u4_lev_suffix_size = 4;
        else if(15 <= u4_lev_prefix)
        {
            u2_lev_code += 15;
            u4_lev_suffix_size = u4_lev_prefix - 3;
        }
        else
            u4_lev_suffix_size = 0;

        //HP_LEVEL_PREFIX
        if(16 <= u4_lev_prefix)
        {
            u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
        }
        if(u4_lev_suffix_size)
        {
            GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                    u4_lev_suffix_size);
            u2_lev_code += u4_lev_suffix;
        }

        u2_abs_value = (u2_lev_code + 2) >> 1;
        /*********************************************************/
        /* If Level code is odd, level is negative else positive */
        /*********************************************************/
        i2_level = (u2_lev_code & 1) ? -u2_abs_value : u2_abs_value;

    }

    /****************************************************************/
    /* Decoding total zeros as in section 9.2.3, table 9.7          */
    /****************************************************************/
    FIND_ONE_IN_STREAM_LEN(u4_ldz, u4_bitstream_offset, pu4_bitstrm_buf, 8);

    if(u4_ldz)
    {
        GETBIT(u4_tot_zero, u4_bitstream_offset, pu4_bitstrm_buf);
        u4_tot_zero = (u4_ldz << 1) - u4_tot_zero;
    }
    else
        u4_tot_zero = 0;

    /***********************************************************************/
    /* Inverse scan and store  residual coeff. Update the bitstream u4_ofst */
    /***********************************************************************/
    u4_scan_pos = u4_tot_zero + u4_isdc;
    if(u4_scan_pos > 15)
        return -1;

    SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
    *pi2_coeff_data++ = i2_level;


    {
        WORD32 offset;
        offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_4x4;
        offset = ALIGN4(offset);
        ps_dec->pv_parse_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_parse_tu_coeff_data + offset);
    }

    ps_bitstrm->u4_ofst = u4_bitstream_offset;
    return 0;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cavlc_4x4res_block_totalcoeff_2to10                      */
/*                                                                           */
/*  Description   : This function does cavlc decoding of 4x4 block residual  */
/*                  coefficient when total coeffs are between two and ten    */
/*                  inclusive. Parsing is done as defined in section 9.2.2   */
/*                  and 9.2.3 the H264 standard.                             */
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
/*         25 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_cavlc_4x4res_block_totalcoeff_2to10(UWORD32 u4_isdc,
                                               UWORD32 u4_total_coeff_trail_one, /*!<TotalCoefficients<<16+trailingones*/
                                               dec_bit_stream_t *ps_bitstrm)
{
    UWORD32 u4_total_zeroes;
    WORD32 i;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bitstream_offset = ps_bitstrm->u4_ofst;
    UWORD32 u4_trailing_ones = u4_total_coeff_trail_one & 0xFFFF;
    UWORD32 u4_total_coeff = u4_total_coeff_trail_one >> 16;
    // To avoid error check at 4x4 level, allocating for 3 extra levels(16+3)
    // since u4_trailing_ones can at the max be 3. This will be required when
    // u4_total_coeff is less than u4_trailing_ones
    WORD16 ai2_level_arr[19];
    WORD16 *i2_level_arr = &ai2_level_arr[3];

    tu_sblk4x4_coeff_data_t *ps_tu_4x4;
    WORD16 *pi2_coeff_data;
    dec_struct_t *ps_dec = (dec_struct_t *)ps_bitstrm->pv_codec_handle;

    ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;
    ps_tu_4x4->u2_sig_coeff_map = 0;
    pi2_coeff_data = &ps_tu_4x4->ai2_level[0];

    i = u4_total_coeff - 1;

    if(u4_trailing_ones)
    {
        /*********************************************************************/
        /* Decode Trailing Ones                                              */
        /* read the sign of T1's and put them in level array                 */
        /*********************************************************************/
        UWORD32 u4_signs, u4_cnt = u4_trailing_ones;
        WORD16 (*ppi2_trlone_lkup)[3] =
                        (WORD16 (*)[3])gai2_ih264d_trailing_one_level;
        WORD16 *pi2_trlone_lkup;

        GETBITS(u4_signs, u4_bitstream_offset, pu4_bitstrm_buf, u4_cnt);

        pi2_trlone_lkup = ppi2_trlone_lkup[(1 << u4_cnt) - 2 + u4_signs];

        while(u4_cnt)
        {
            i2_level_arr[i--] = *pi2_trlone_lkup++;
            u4_cnt--;
        }
    }

    /****************************************************************/
    /* Decoding Levels Begins                                       */
    /****************************************************************/
    if(i >= 0)
    {
        /****************************************************************/
        /* First level is decoded outside the loop as it has lot of     */
        /* special cases.                                               */
        /****************************************************************/
        UWORD32 u4_lev_suffix, u4_suffix_len, u4_lev_suffix_size;
        WORD32 u2_lev_code, u2_abs_value;
        UWORD32 u4_lev_prefix;

        /***************************************************************/
        /* u4_suffix_len = 0,  Find leading zeros in next 32 bits      */
        /***************************************************************/
        FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                              pu4_bitstrm_buf);

        /*********************************************************/
        /* Special decoding case when trailing ones are 3        */
        /*********************************************************/
        u2_lev_code = MIN(15, u4_lev_prefix);

        u2_lev_code += (3 == u4_trailing_ones) ? 0 : 2;

        if(14 == u4_lev_prefix)
            u4_lev_suffix_size = 4;
        else if(15 <= u4_lev_prefix)
        {
            u2_lev_code += 15;
            u4_lev_suffix_size = u4_lev_prefix - 3;
        }
        else
            u4_lev_suffix_size = 0;

        //HP_LEVEL_PREFIX
        if(16 <= u4_lev_prefix)
        {
            u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
        }
        if(u4_lev_suffix_size)
        {
            GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                    u4_lev_suffix_size);
            u2_lev_code += u4_lev_suffix;
        }

        u2_abs_value = (u2_lev_code + 2) >> 1;
        /*********************************************************/
        /* If Level code is odd, level is negative else positive */
        /*********************************************************/
        i2_level_arr[i--] = (u2_lev_code & 1) ? -u2_abs_value : u2_abs_value;

        u4_suffix_len = (u2_abs_value > 3) ? 2 : 1;

        /*********************************************************/
        /* Now loop over the remaining levels                    */
        /*********************************************************/
        while(i >= 0)
        {

            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                                  pu4_bitstrm_buf);

            u4_lev_suffix_size =
                            (15 <= u4_lev_prefix) ?
                                            (u4_lev_prefix - 3) : u4_suffix_len;

            /*********************************************************/
            /* Compute level code using prefix and suffix            */
            /*********************************************************/
            GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                    u4_lev_suffix_size);
            u2_lev_code = (MIN(15,u4_lev_prefix) << u4_suffix_len)
                            + u4_lev_suffix;

            //HP_LEVEL_PREFIX
            if(16 <= u4_lev_prefix)
            {
                u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
            }
            u2_abs_value = (u2_lev_code + 2) >> 1;

            /*********************************************************/
            /* If Level code is odd, level is negative else positive */
            /*********************************************************/
            i2_level_arr[i--] =
                            (u2_lev_code & 1) ? -u2_abs_value : u2_abs_value;

            /*********************************************************/
            /* Increment suffix length if required                   */
            /*********************************************************/
            u4_suffix_len +=
                            (u4_suffix_len < 6) ?
                                            (u2_abs_value
                                                            > (3
                                                                            << (u4_suffix_len
                                                                                            - 1))) :
                                            0;
        }

        /****************************************************************/
        /* Decoding Levels Ends                                         */
        /****************************************************************/
    }

    /****************************************************************/
    /* Decoding total zeros as in section 9.2.3, table 9.7          */
    /****************************************************************/
    {
        UWORD32 u4_index;
        const UWORD8 (*ppu1_total_zero_lkup)[64] =
                        (const UWORD8 (*)[64])gau1_ih264d_table_total_zero_2to10;

        NEXTBITS(u4_index, u4_bitstream_offset, pu4_bitstrm_buf, 6);
        u4_total_zeroes = ppu1_total_zero_lkup[u4_total_coeff - 2][u4_index];

        FLUSHBITS(u4_bitstream_offset, (u4_total_zeroes >> 4));
        u4_total_zeroes &= 0xf;
    }

    /**************************************************************/
    /* Decode the runs and form the coefficient buffer            */
    /**************************************************************/
    {
        const UWORD8 *pu1_table_runbefore;
        UWORD32 u4_run;
        WORD32 k;
        WORD32 u4_scan_pos = u4_total_coeff + u4_total_zeroes - 1 + u4_isdc;
        WORD32 u4_zeroes_left = u4_total_zeroes;
        k = u4_total_coeff - 1;

        /**************************************************************/
        /* Decoding Runs Begin for zeros left > 6                     */
        /**************************************************************/
        while((u4_zeroes_left > 6) && k)
        {
            UWORD32 u4_code;

            NEXTBITS(u4_code, u4_bitstream_offset, pu4_bitstrm_buf, 3);

            if(u4_code != 0)
            {
                FLUSHBITS(u4_bitstream_offset, 3);
                u4_run = (7 - u4_code);
            }
            else
            {

                FIND_ONE_IN_STREAM_LEN(u4_code, u4_bitstream_offset,
                                       pu4_bitstrm_buf, 11);
                u4_run = (4 + u4_code);
            }

            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
            *pi2_coeff_data++ = i2_level_arr[k--];
            u4_zeroes_left -= (WORD32)u4_run;
            u4_scan_pos -= (WORD32)(u4_run + 1);
        }

        if (u4_zeroes_left < 0 || u4_scan_pos < 0)
            return -1;

        /**************************************************************/
        /* Decoding Runs for 0 < zeros left <=6                       */
        /**************************************************************/
        pu1_table_runbefore = (UWORD8 *)gau1_ih264d_table_run_before;
        while((u4_zeroes_left > 0) && k)
        {
            UWORD32 u4_code;
            NEXTBITS(u4_code, u4_bitstream_offset, pu4_bitstrm_buf, 3);

            u4_code = pu1_table_runbefore[u4_code + (u4_zeroes_left << 3)];
            u4_run = u4_code >> 2;

            FLUSHBITS(u4_bitstream_offset, (u4_code & 0x03));

            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
            *pi2_coeff_data++ = i2_level_arr[k--];
            u4_zeroes_left -= (WORD32)u4_run;
            u4_scan_pos -= (WORD32)(u4_run + 1);
        }
        if (u4_zeroes_left < 0 || u4_scan_pos < 0)
            return -1;
        /**************************************************************/
        /* Decoding Runs End                                          */
        /**************************************************************/

        /**************************************************************/
        /* Copy the remaining coefficients                            */
        /**************************************************************/
        while(k >= 0)
        {

            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
            *pi2_coeff_data++ = i2_level_arr[k--];
            u4_scan_pos--;
        }
    }

    {
        WORD32 offset;
        offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_4x4;
        offset = ALIGN4(offset);
        ps_dec->pv_parse_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_parse_tu_coeff_data + offset);
    }

    ps_bitstrm->u4_ofst = u4_bitstream_offset;
    return 0;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cavlc_4x4res_block_totalcoeff_11to16                     */
/*                                                                           */
/*  Description   : This function does cavlc decoding of 4x4 block residual  */
/*                  coefficient when total coeffs are greater than ten.      */
/*                  Parsing is done as defined in section 9.2.2 and 9.2.3 of */
/*                  the H264 standard.                                       */
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
/*         25 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_cavlc_4x4res_block_totalcoeff_11to16(UWORD32 u4_isdc,
                                                UWORD32 u4_total_coeff_trail_one, /*!<TotalCoefficients<<16+trailingones*/
                                                dec_bit_stream_t *ps_bitstrm )
{
    UWORD32 u4_total_zeroes;
    WORD32 i;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bitstream_offset = ps_bitstrm->u4_ofst;
    UWORD32 u4_trailing_ones = u4_total_coeff_trail_one & 0xFFFF;
    UWORD32 u4_total_coeff = u4_total_coeff_trail_one >> 16;
    // To avoid error check at 4x4 level, allocating for 3 extra levels(16+3)
    // since u4_trailing_ones can at the max be 3. This will be required when
    // u4_total_coeff is less than u4_trailing_ones
    WORD16 ai2_level_arr[19];//
    WORD16 *i2_level_arr = &ai2_level_arr[3];

    tu_sblk4x4_coeff_data_t *ps_tu_4x4;
    WORD16 *pi2_coeff_data;
    dec_struct_t *ps_dec = (dec_struct_t *)ps_bitstrm->pv_codec_handle;

    ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;
    ps_tu_4x4->u2_sig_coeff_map = 0;
    pi2_coeff_data = &ps_tu_4x4->ai2_level[0];

    i = u4_total_coeff - 1;
    if(u4_trailing_ones)
    {
        /*********************************************************************/
        /* Decode Trailing Ones                                              */
        /* read the sign of T1's and put them in level array                 */
        /*********************************************************************/
        UWORD32 u4_signs, u4_cnt = u4_trailing_ones;
        WORD16 (*ppi2_trlone_lkup)[3] =
                        (WORD16 (*)[3])gai2_ih264d_trailing_one_level;
        WORD16 *pi2_trlone_lkup;

        GETBITS(u4_signs, u4_bitstream_offset, pu4_bitstrm_buf, u4_cnt);

        pi2_trlone_lkup = ppi2_trlone_lkup[(1 << u4_cnt) - 2 + u4_signs];

        while(u4_cnt)
        {
            i2_level_arr[i--] = *pi2_trlone_lkup++;
            u4_cnt--;
        }
    }

    /****************************************************************/
    /* Decoding Levels Begins                                       */
    /****************************************************************/
    if(i >= 0)
    {
        /****************************************************************/
        /* First level is decoded outside the loop as it has lot of     */
        /* special cases.                                               */
        /****************************************************************/
        UWORD32 u4_lev_suffix, u4_suffix_len, u4_lev_suffix_size;
        UWORD16 u2_lev_code, u2_abs_value;
        UWORD32 u4_lev_prefix;

        if(u4_trailing_ones < 3)
        {
            /*********************************************************/
            /* u4_suffix_len = 1                                     */
            /*********************************************************/
            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                                  pu4_bitstrm_buf);

            u4_lev_suffix_size =
                            (15 <= u4_lev_prefix) ? (u4_lev_prefix - 3) : 1;

            GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                    u4_lev_suffix_size);
            u2_lev_code = 2 + (MIN(u4_lev_prefix,15) << 1) + u4_lev_suffix;

            //HP_LEVEL_PREFIX
            if(16 <= u4_lev_prefix)
            {
                u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
            }
        }
        else
        {
            /*********************************************************/
            /*u4_suffix_len = 0                                      */
            /*********************************************************/
            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                                  pu4_bitstrm_buf);

            /*********************************************************/
            /* Special decoding case when trailing ones are 3        */
            /*********************************************************/
            u2_lev_code = MIN(15, u4_lev_prefix);

            u2_lev_code += (3 == u4_trailing_ones) ? 0 : (2);

            if(14 == u4_lev_prefix)
                u4_lev_suffix_size = 4;
            else if(15 <= u4_lev_prefix)
            {
                u2_lev_code += 15;
                u4_lev_suffix_size = (u4_lev_prefix - 3);
            }
            else
                u4_lev_suffix_size = 0;

            //HP_LEVEL_PREFIX
            if(16 <= u4_lev_prefix)
            {
                u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
            }
            if(u4_lev_suffix_size)
            {
                GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                        u4_lev_suffix_size);
                u2_lev_code += u4_lev_suffix;
            }
        }

        u2_abs_value = (u2_lev_code + 2) >> 1;
        /*********************************************************/
        /* If Level code is odd, level is negative else positive */
        /*********************************************************/
        i2_level_arr[i--] = (u2_lev_code & 1) ? -u2_abs_value : u2_abs_value;

        u4_suffix_len = (u2_abs_value > 3) ? 2 : 1;

        /*********************************************************/
        /* Now loop over the remaining levels                    */
        /*********************************************************/
        while(i >= 0)
        {

            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                                  pu4_bitstrm_buf);

            u4_lev_suffix_size =
                            (15 <= u4_lev_prefix) ?
                                            (u4_lev_prefix - 3) : u4_suffix_len;

            /*********************************************************/
            /* Compute level code using prefix and suffix            */
            /*********************************************************/
            GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                    u4_lev_suffix_size);
            u2_lev_code = (MIN(15,u4_lev_prefix) << u4_suffix_len)
                            + u4_lev_suffix;

            //HP_LEVEL_PREFIX
            if(16 <= u4_lev_prefix)
            {
                u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
            }
            u2_abs_value = (u2_lev_code + 2) >> 1;

            /*********************************************************/
            /* If Level code is odd, level is negative else positive */
            /*********************************************************/
            i2_level_arr[i--] =
                            (u2_lev_code & 1) ? -u2_abs_value : u2_abs_value;

            /*********************************************************/
            /* Increment suffix length if required                   */
            /*********************************************************/
            u4_suffix_len +=
                            (u4_suffix_len < 6) ?
                                            (u2_abs_value
                                                            > (3
                                                                            << (u4_suffix_len
                                                                                            - 1))) :
                                            0;
        }

        /****************************************************************/
        /* Decoding Levels Ends                                         */
        /****************************************************************/
    }

    if(u4_total_coeff < (16 - u4_isdc))
    {
        UWORD32 u4_index;
        const UWORD8 (*ppu1_total_zero_lkup)[16] =
                        (const UWORD8 (*)[16])gau1_ih264d_table_total_zero_11to15;

        NEXTBITS(u4_index, u4_bitstream_offset, pu4_bitstrm_buf, 4);
        u4_total_zeroes = ppu1_total_zero_lkup[u4_total_coeff - 11][u4_index];

        FLUSHBITS(u4_bitstream_offset, (u4_total_zeroes >> 4));
        u4_total_zeroes &= 0xf;
    }
    else
        u4_total_zeroes = 0;

    /**************************************************************/
    /* Decode the runs and form the coefficient buffer            */
    /**************************************************************/
    {
        const UWORD8 *pu1_table_runbefore;
        UWORD32 u4_run;
        WORD32 k;
        WORD32 u4_scan_pos = u4_total_coeff + u4_total_zeroes - 1 + u4_isdc;
        WORD32 u4_zeroes_left = u4_total_zeroes;
        k = u4_total_coeff - 1;

        /**************************************************************/
        /* Decoding Runs for 0 < zeros left <=6                       */
        /**************************************************************/
        pu1_table_runbefore = (UWORD8 *)gau1_ih264d_table_run_before;
        while((u4_zeroes_left > 0) && k)
        {
            UWORD32 u4_code;
            NEXTBITS(u4_code, u4_bitstream_offset, pu4_bitstrm_buf, 3);

            u4_code = pu1_table_runbefore[u4_code + (u4_zeroes_left << 3)];
            u4_run = u4_code >> 2;

            FLUSHBITS(u4_bitstream_offset, (u4_code & 0x03));
            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
            *pi2_coeff_data++ = i2_level_arr[k--];
            u4_zeroes_left -= (WORD32)u4_run;
            u4_scan_pos -= (WORD32)(u4_run + 1);
        }
        if (u4_zeroes_left < 0 || u4_scan_pos < 0)
          return -1;

        /**************************************************************/
        /* Decoding Runs End                                          */
        /**************************************************************/

        /**************************************************************/
        /* Copy the remaining coefficients                            */
        /**************************************************************/
        while(k >= 0)
        {
            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
            *pi2_coeff_data++ = i2_level_arr[k--];
            u4_scan_pos--;
        }
    }

    {
        WORD32 offset;
        offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_4x4;
        offset = ALIGN4(offset);
        ps_dec->pv_parse_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_parse_tu_coeff_data + offset);
    }

    ps_bitstrm->u4_ofst = u4_bitstream_offset;
    return 0;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_rest_of_residual_cav_chroma_dc_block              */
/*                                                                           */
/*  Description   : This function does the Cavlc parsing of the bitstream    */
/*                  for chroma dc coefficients                               */
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
/*         15 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
void ih264d_rest_of_residual_cav_chroma_dc_block(UWORD32 u4_total_coeff_trail_one,
                                                 dec_bit_stream_t *ps_bitstrm)
{
    UWORD32 u4_total_zeroes;
    WORD16 i;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bitstream_offset = ps_bitstrm->u4_ofst;
    UWORD32 u4_trailing_ones = u4_total_coeff_trail_one & 0xFFFF;
    UWORD32 u4_total_coeff = u4_total_coeff_trail_one >> 16;
    // To avoid error check at 4x4 level, allocating for 3 extra levels(4+3)
    // since u4_trailing_ones can at the max be 3. This will be required when
    // u4_total_coeff is less than u4_trailing_ones
    WORD16 ai2_level_arr[7];//
    WORD16 *i2_level_arr = &ai2_level_arr[3];

    tu_sblk4x4_coeff_data_t *ps_tu_4x4;
    WORD16 *pi2_coeff_data;
    dec_struct_t *ps_dec = (dec_struct_t *)ps_bitstrm->pv_codec_handle;

    ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;
    ps_tu_4x4->u2_sig_coeff_map = 0;
    pi2_coeff_data = &ps_tu_4x4->ai2_level[0];

    i = u4_total_coeff - 1;
    if(u4_trailing_ones)
    {
        /*********************************************************************/
        /* Decode Trailing Ones                                              */
        /* read the sign of T1's and put them in level array                 */
        /*********************************************************************/
        UWORD32 u4_signs, u4_cnt = u4_trailing_ones;
        WORD16 (*ppi2_trlone_lkup)[3] =
                        (WORD16 (*)[3])gai2_ih264d_trailing_one_level;
        WORD16 *pi2_trlone_lkup;

        GETBITS(u4_signs, u4_bitstream_offset, pu4_bitstrm_buf, u4_cnt);

        pi2_trlone_lkup = ppi2_trlone_lkup[(1 << u4_cnt) - 2 + u4_signs];

        while(u4_cnt)
        {
            i2_level_arr[i--] = *pi2_trlone_lkup++;
            u4_cnt--;
        }
    }

    /****************************************************************/
    /* Decoding Levels Begins                                       */
    /****************************************************************/
    if(i >= 0)
    {
        /****************************************************************/
        /* First level is decoded outside the loop as it has lot of     */
        /* special cases.                                               */
        /****************************************************************/
        UWORD32 u4_lev_suffix, u4_suffix_len, u4_lev_suffix_size;
        UWORD16 u2_lev_code, u2_abs_value;
        UWORD32 u4_lev_prefix;

        /***************************************************************/
        /* u4_suffix_len = 0,  Find leading zeros in next 32 bits      */
        /***************************************************************/
        FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                              pu4_bitstrm_buf);

        /*********************************************************/
        /* Special decoding case when trailing ones are 3        */
        /*********************************************************/
        u2_lev_code = MIN(15, u4_lev_prefix);

        u2_lev_code += (3 == u4_trailing_ones) ? 0 : (2);

        if(14 == u4_lev_prefix)
            u4_lev_suffix_size = 4;
        else if(15 <= u4_lev_prefix)
        {
            u2_lev_code += 15;
            u4_lev_suffix_size = u4_lev_prefix - 3;
        }
        else
            u4_lev_suffix_size = 0;

        //HP_LEVEL_PREFIX
        if(16 <= u4_lev_prefix)
        {
            u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
        }
        if(u4_lev_suffix_size)
        {
            GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                    u4_lev_suffix_size);
            u2_lev_code += u4_lev_suffix;
        }

        u2_abs_value = (u2_lev_code + 2) >> 1;
        /*********************************************************/
        /* If Level code is odd, level is negative else positive */
        /*********************************************************/
        i2_level_arr[i--] = (u2_lev_code & 1) ? -u2_abs_value : u2_abs_value;

        u4_suffix_len = (u2_abs_value > 3) ? 2 : 1;

        /*********************************************************/
        /* Now loop over the remaining levels                    */
        /*********************************************************/
        while(i >= 0)
        {

            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            FIND_ONE_IN_STREAM_32(u4_lev_prefix, u4_bitstream_offset,
                                  pu4_bitstrm_buf);

            u4_lev_suffix_size =
                            (15 <= u4_lev_prefix) ?
                                            (u4_lev_prefix - 3) : u4_suffix_len;

            /*********************************************************/
            /* Compute level code using prefix and suffix            */
            /*********************************************************/
            GETBITS(u4_lev_suffix, u4_bitstream_offset, pu4_bitstrm_buf,
                    u4_lev_suffix_size);
            u2_lev_code = (MIN(u4_lev_prefix,15) << u4_suffix_len)
                            + u4_lev_suffix;

            //HP_LEVEL_PREFIX
            if(16 <= u4_lev_prefix)
            {
                u2_lev_code += ((1 << (u4_lev_prefix - 3)) - 4096);
            }
            u2_abs_value = (u2_lev_code + 2) >> 1;

            /*********************************************************/
            /* If Level code is odd, level is negative else positive */
            /*********************************************************/
            i2_level_arr[i--] =
                            (u2_lev_code & 1) ? -u2_abs_value : u2_abs_value;

            /*********************************************************/
            /* Increment suffix length if required                   */
            /*********************************************************/
            u4_suffix_len += (u2_abs_value > (3 << (u4_suffix_len - 1)));
        }

        /****************************************************************/
        /* Decoding Levels Ends                                         */
        /****************************************************************/
    }

    if(u4_total_coeff < 4)
    {
        UWORD32 u4_max_ldz = (4 - u4_total_coeff);
        FIND_ONE_IN_STREAM_LEN(u4_total_zeroes, u4_bitstream_offset,
                               pu4_bitstrm_buf, u4_max_ldz);
    }
    else
        u4_total_zeroes = 0;

    /**************************************************************/
    /* Decode the runs and form the coefficient buffer            */
    /**************************************************************/
    {
        const UWORD8 *pu1_table_runbefore;
        UWORD32 u4_run;
        WORD32 u4_scan_pos = (u4_total_coeff + u4_total_zeroes - 1);
        UWORD32 u4_zeroes_left = u4_total_zeroes;
        i = u4_total_coeff - 1;

        /**************************************************************/
        /* Decoding Runs for 0 < zeros left <=6                       */
        /**************************************************************/
        pu1_table_runbefore = (UWORD8 *)gau1_ih264d_table_run_before;
        while(u4_zeroes_left && i)
        {
            UWORD32 u4_code;
            NEXTBITS(u4_code, u4_bitstream_offset, pu4_bitstrm_buf, 3);

            u4_code = pu1_table_runbefore[u4_code + (u4_zeroes_left << 3)];
            u4_run = u4_code >> 2;

            FLUSHBITS(u4_bitstream_offset, (u4_code & 0x03));
            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
            *pi2_coeff_data++ = i2_level_arr[i--];
            u4_zeroes_left -= (WORD32)u4_run;
            u4_scan_pos -= (WORD32)(u4_run + 1);
        }
        /**************************************************************/
        /* Decoding Runs End                                          */
        /**************************************************************/

        /**************************************************************/
        /* Copy the remaining coefficients                            */
        /**************************************************************/
        while(i >= 0)
        {
            SET_BIT(ps_tu_4x4->u2_sig_coeff_map, u4_scan_pos);
            *pi2_coeff_data++ = i2_level_arr[i--];
            u4_scan_pos--;
        }
    }

    {
        WORD32 offset;
        offset = (UWORD8 *)pi2_coeff_data - (UWORD8 *)ps_tu_4x4;
        offset = ALIGN4(offset);
        ps_dec->pv_parse_tu_coeff_data = (void *)((UWORD8 *)ps_dec->pv_parse_tu_coeff_data + offset);
    }

    ps_bitstrm->u4_ofst = u4_bitstream_offset;
}

/*!
 **************************************************************************
 * \if Function name : CavlcParsingInvScanInvQuant \endif
 *
 * \brief
 *    This function do cavlc parsing of coefficient tokens for any block
 *    type except chromDc and depending
 *    on whenther any coefficients to be parsed calls module
 *    RestOfResidualBlockCavlc.
 *
 * \return
 *    Returns total number of non-zero coefficients.
 *
 **************************************************************************
 */

WORD32 ih264d_cavlc_parse4x4coeff_n0to7(WORD16 *pi2_coeff_block,
                                        UWORD32 u4_isdc, /* is it a DC block */
                                        WORD32 u4_n,
                                        dec_struct_t *ps_dec,
                                        UWORD32 *pu4_total_coeff)
{
    dec_bit_stream_t *ps_bitstrm = ps_dec->ps_bitstrm;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bitstream_offset = ps_bitstrm->u4_ofst;
    UWORD32 u4_code, u4_index, u4_ldz;
    const UWORD16 *pu2_code = (const UWORD16*)gau2_ih264d_code_gx;
    const UWORD16 *pu2_offset_num_vlc =
                    (const UWORD16 *)gau2_ih264d_offset_num_vlc_tab;
    UWORD32 u4_offset_num_vlc = pu2_offset_num_vlc[u4_n];


    UNUSED(pi2_coeff_block);
    *pu4_total_coeff = 0;
    FIND_ONE_IN_STREAM_32(u4_ldz, u4_bitstream_offset, pu4_bitstrm_buf);
    NEXTBITS(u4_index, u4_bitstream_offset, pu4_bitstrm_buf, 3);
    u4_index += (u4_ldz << 3);
    u4_index += u4_offset_num_vlc;

    u4_index = MIN(u4_index, 303);
    u4_code = pu2_code[u4_index];

    FLUSHBITS(u4_bitstream_offset, (u4_code & 0x03));
    ps_bitstrm->u4_ofst = u4_bitstream_offset;
    *pu4_total_coeff = (u4_code >> 4);

    if(*pu4_total_coeff)
    {
        UWORD32 u4_trailing_ones, u4_offset, u4_total_coeff_tone;
        const UWORD8 *pu1_offset =
                        (UWORD8 *)gau1_ih264d_total_coeff_fn_ptr_offset;
        WORD32 ret;
        u4_trailing_ones = ((u4_code >> 2) & 0x03);
        u4_offset = pu1_offset[*pu4_total_coeff - 1];
        u4_total_coeff_tone = (*pu4_total_coeff << 16) | u4_trailing_ones;

        ret = ps_dec->pf_cavlc_4x4res_block[u4_offset](u4_isdc,
                                                       u4_total_coeff_tone,
                                                       ps_bitstrm);
        if(ret != 0)
            return ERROR_CAVLC_NUM_COEFF_T;
    }

    return OK;
}

WORD32 ih264d_cavlc_parse4x4coeff_n8(WORD16 *pi2_coeff_block,
                                     UWORD32 u4_isdc, /* is it a DC block */
                                     WORD32 u4_n,
                                     dec_struct_t *ps_dec,
                                     UWORD32 *pu4_total_coeff)
{

    dec_bit_stream_t *ps_bitstrm = ps_dec->ps_bitstrm;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bitstream_offset = ps_bitstrm->u4_ofst;
    UWORD32 u4_code;
    UNUSED(u4_n);
    UNUSED(pi2_coeff_block);
    GETBITS(u4_code, u4_bitstream_offset, pu4_bitstrm_buf, 6);
    ps_bitstrm->u4_ofst = u4_bitstream_offset;
    *pu4_total_coeff = 0;

    if(u4_code != 3)
    {
        UWORD8 *pu1_offset = (UWORD8 *)gau1_ih264d_total_coeff_fn_ptr_offset;
        UWORD32 u4_trailing_ones, u4_offset, u4_total_coeff_tone;

        *pu4_total_coeff = (u4_code >> 2) + 1;
        u4_trailing_ones = u4_code & 0x03;
        u4_offset = pu1_offset[*pu4_total_coeff - 1];
        u4_total_coeff_tone = (*pu4_total_coeff << 16) | u4_trailing_ones;

        ps_dec->pf_cavlc_4x4res_block[u4_offset](u4_isdc,
                                                 u4_total_coeff_tone,
                                                 ps_bitstrm);
    }

    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_cavlc_parse_chroma_dc \endif
 *
 * \brief
 *    This function do cavlc parsing of coefficient tokens chromDc block
 *    and depending  on whenther any coefficients to be parsed calls module
 *    ih264d_rest_of_residual_cav_chroma_dc_block.
 *
 * \return
 *    Returns total number of non-zero coefficients.
 *
 **************************************************************************
 */

void ih264d_cavlc_parse_chroma_dc(dec_mb_info_t *ps_cur_mb_info,
                                  WORD16 *pi2_coeff_block,
                                  dec_bit_stream_t *ps_bitstrm,
                                  UWORD32 u4_scale_u,
                                  UWORD32 u4_scale_v,
                                  WORD32 i4_mb_inter_inc)
{
    UWORD32 u4_total_coeff, u4_trailing_ones, u4_total_coeff_tone, u4_code;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bitstream_offset = ps_bitstrm->u4_ofst;
    const UWORD8 *pu1_cav_chromdc = (const UWORD8*)gau1_ih264d_cav_chromdc_vld;
    UNUSED(i4_mb_inter_inc);
    /******************************************************************/
    /*  Chroma DC Block for U component                               */
    /******************************************************************/
    NEXTBITS(u4_code, u4_bitstream_offset, pu4_bitstrm_buf, 8);

    u4_code = pu1_cav_chromdc[u4_code];

    FLUSHBITS(u4_bitstream_offset, ((u4_code & 0x7) + 1));
    ps_bitstrm->u4_ofst = u4_bitstream_offset;

    u4_total_coeff = (u4_code >> 5);

    if(u4_total_coeff)
    {
        WORD32 i_z0, i_z1, i_z2, i_z3;
        tu_sblk4x4_coeff_data_t *ps_tu_4x4;
        dec_struct_t *ps_dec = (dec_struct_t *)ps_bitstrm->pv_codec_handle;
        WORD16 ai2_dc_coef[4];
        UWORD8 pu1_inv_scan[4] =
                        { 0, 1, 2, 3 };
        WORD16 *pi2_coeff_data =
                                    (WORD16 *)ps_dec->pv_parse_tu_coeff_data;

        ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;

        u4_trailing_ones = ((u4_code >> 3) & 0x3);
        u4_total_coeff_tone = (u4_total_coeff << 16) | u4_trailing_ones;
        ih264d_rest_of_residual_cav_chroma_dc_block(u4_total_coeff_tone,
                                                    ps_bitstrm);

        ai2_dc_coef[0] = 0;
        ai2_dc_coef[1] = 0;
        ai2_dc_coef[2] = 0;
        ai2_dc_coef[3] = 0;

        ih264d_unpack_coeff4x4_dc_4x4blk(ps_tu_4x4,
                                         ai2_dc_coef,
                                         pu1_inv_scan);
        /*-------------------------------------------------------------------*/
        /* Inverse 2x2 transform and scaling  of chroma DC                   */
        /*-------------------------------------------------------------------*/
        i_z0 = (ai2_dc_coef[0] + ai2_dc_coef[2]);
        i_z1 = (ai2_dc_coef[0] - ai2_dc_coef[2]);
        i_z2 = (ai2_dc_coef[1] - ai2_dc_coef[3]);
        i_z3 = (ai2_dc_coef[1] + ai2_dc_coef[3]);

        /*-----------------------------------------------------------*/
        /* Scaling and storing the values back                       */
        /*-----------------------------------------------------------*/
        *pi2_coeff_data++ = (WORD16)(((i_z0 + i_z3) * (WORD32)u4_scale_u) >> 5);
        *pi2_coeff_data++ = (WORD16)(((i_z0 - i_z3) * (WORD32)u4_scale_u) >> 5);
        *pi2_coeff_data++ = (WORD16)(((i_z1 + i_z2) * (WORD32)u4_scale_u) >> 5);
        *pi2_coeff_data++ = (WORD16)(((i_z1 - i_z2) * (WORD32)u4_scale_u) >> 5);

        ps_dec->pv_parse_tu_coeff_data = (void *)pi2_coeff_data;

        SET_BIT(ps_cur_mb_info->u1_yuv_dc_block_flag,1);
    }

    /******************************************************************/
    /*  Chroma DC Block for V component                               */
    /******************************************************************/
    pi2_coeff_block += 64;
    u4_bitstream_offset = ps_bitstrm->u4_ofst;

    NEXTBITS(u4_code, u4_bitstream_offset, pu4_bitstrm_buf, 8);

    u4_code = pu1_cav_chromdc[u4_code];

    FLUSHBITS(u4_bitstream_offset, ((u4_code & 0x7) + 1));
    ps_bitstrm->u4_ofst = u4_bitstream_offset;

    u4_total_coeff = (u4_code >> 5);

    if(u4_total_coeff)
    {
        WORD32 i_z0, i_z1, i_z2, i_z3;
        tu_sblk4x4_coeff_data_t *ps_tu_4x4;
        dec_struct_t *ps_dec = (dec_struct_t *)ps_bitstrm->pv_codec_handle;
        WORD16 ai2_dc_coef[4];
        UWORD8 pu1_inv_scan[4] =
                        { 0, 1, 2, 3 };
        WORD16 *pi2_coeff_data =
                                    (WORD16 *)ps_dec->pv_parse_tu_coeff_data;

        ps_tu_4x4 = (tu_sblk4x4_coeff_data_t *)ps_dec->pv_parse_tu_coeff_data;

        u4_trailing_ones = ((u4_code >> 3) & 0x3);
        u4_total_coeff_tone = (u4_total_coeff << 16) | u4_trailing_ones;
        ih264d_rest_of_residual_cav_chroma_dc_block(u4_total_coeff_tone,
                                                    ps_bitstrm);

        ai2_dc_coef[0] = 0;
        ai2_dc_coef[1] = 0;
        ai2_dc_coef[2] = 0;
        ai2_dc_coef[3] = 0;

        ih264d_unpack_coeff4x4_dc_4x4blk(ps_tu_4x4,
                                         ai2_dc_coef,
                                         pu1_inv_scan);

        /*-------------------------------------------------------------------*/
        /* Inverse 2x2 transform and scaling  of chroma DC                   */
        /*-------------------------------------------------------------------*/
        i_z0 = (ai2_dc_coef[0] + ai2_dc_coef[2]);
        i_z1 = (ai2_dc_coef[0] - ai2_dc_coef[2]);
        i_z2 = (ai2_dc_coef[1] - ai2_dc_coef[3]);
        i_z3 = (ai2_dc_coef[1] + ai2_dc_coef[3]);

        /*-----------------------------------------------------------*/
        /* Scaling and storing the values back                       */
        /*-----------------------------------------------------------*/
        *pi2_coeff_data++ = (WORD16)(((i_z0 + i_z3) * (WORD32)u4_scale_v) >> 5);
        *pi2_coeff_data++ = (WORD16)(((i_z0 - i_z3) * (WORD32)u4_scale_v) >> 5);
        *pi2_coeff_data++ = (WORD16)(((i_z1 + i_z2) * (WORD32)u4_scale_v) >> 5);
        *pi2_coeff_data++ = (WORD16)(((i_z1 - i_z2) * (WORD32)u4_scale_v) >> 5);

        ps_dec->pv_parse_tu_coeff_data = (void *)pi2_coeff_data;

        SET_BIT(ps_cur_mb_info->u1_yuv_dc_block_flag,2);
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_pmb_ref_index_cavlc_range1                         */
/*                                                                           */
/*  Description   : This function does the Cavlc  TEV range =1 parsing of    */
/*                  reference index  for a P MB. Range is 1 when             */
/*                  num_ref_idx_active_minus1 is 0                           */
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
/*         19 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
void ih264d_parse_pmb_ref_index_cavlc_range1(UWORD32 u4_num_part, /* Number of partitions in MB      */
                                             dec_bit_stream_t *ps_bitstrm, /* Pointer to bitstream Structure. */
                                             WORD8 *pi1_ref_idx, /* pointer to reference index array */
                                             UWORD32 u4_num_ref_idx_active_minus1 /* Not used for range 1    */
                                             )
{
    UWORD32 u4_i;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstream_off = &ps_bitstrm->u4_ofst;
    UNUSED(u4_num_ref_idx_active_minus1);
    for(u4_i = 0; u4_i < u4_num_part; u4_i++)
    {
        UWORD32 u4_ref_idx;
        u4_ref_idx = ih264d_tev_range1(pu4_bitstream_off, pu4_bitstrm_buf);

        /* Storing Reference Idx Information */
        pi1_ref_idx[u4_i] = (WORD8)u4_ref_idx;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_pmb_ref_index_cavlc                                */
/*                                                                           */
/*  Description   : This function does the Cavlc  TEV range > 1 parsing of   */
/*                  reference index  for a P MB.                             */
/*                  Range > 1 when num_ref_idx_active_minus1 > 0             */
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
/*         19 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/

WORD32 ih264d_parse_pmb_ref_index_cavlc(UWORD32 u4_num_part, /* Number of partitions in MB      */
                                      dec_bit_stream_t *ps_bitstrm, /* Pointer to bitstream Structure. */
                                      WORD8 *pi1_ref_idx, /* pointer to reference index array */
                                      UWORD32 u4_num_ref_idx_active_minus1 /* Number of active references - 1  */
                                      )
{
    UWORD32 u4_i;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstream_off = &ps_bitstrm->u4_ofst;

    for(u4_i = 0; u4_i < u4_num_part; u4_i++)
    {
        UWORD32 u4_ref_idx;
//Inlined ih264d_uev
        UWORD32 u4_bitstream_offset = *pu4_bitstream_off;
        UWORD32 u4_word, u4_ldz;

        /***************************************************************/
        /* Find leading zeros in next 32 bits                          */
        /***************************************************************/
        NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
        u4_ldz = CLZ(u4_word);
        /* Flush the ps_bitstrm */
        u4_bitstream_offset += (u4_ldz + 1);
        /* Read the suffix from the ps_bitstrm */
        u4_word = 0;
        if(u4_ldz)
            GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf, u4_ldz);
        *pu4_bitstream_off = u4_bitstream_offset;
        u4_ref_idx = ((1 << u4_ldz) + u4_word - 1);
//Inlined ih264d_uev

        if(u4_ref_idx > u4_num_ref_idx_active_minus1)
            return ERROR_REF_IDX;

        /* Storing Reference Idx Information */
        pi1_ref_idx[u4_i] = (WORD8)u4_ref_idx;
    }
    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_bmb_ref_index_cavlc_range1                         */
/*                                                                           */
/*  Description   : This function does the Cavlc  TEV range =1 parsing of    */
/*                  reference index  for a B MB. Range is 1 when             */
/*                  num_ref_idx_active_minus1 is 0                           */
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
/*         19 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
void ih264d_parse_bmb_ref_index_cavlc_range1(UWORD32 u4_num_part, /* Number of partitions in MB      */
                                             dec_bit_stream_t *ps_bitstrm, /* Pointer to bitstream Structure. */
                                             WORD8 *pi1_ref_idx, /* pointer to reference index array */
                                             UWORD32 u4_num_ref_idx_active_minus1 /* Not used for range 1    */
                                             )
{
    UWORD32 u4_i;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstream_off = &ps_bitstrm->u4_ofst;
    UNUSED(u4_num_ref_idx_active_minus1);
    for(u4_i = 0; u4_i < u4_num_part; u4_i++)
    {
        if(pi1_ref_idx[u4_i] > -1)
        {
            UWORD32 u4_ref_idx;

            u4_ref_idx = ih264d_tev_range1(pu4_bitstream_off, pu4_bitstrm_buf);

            /* Storing Reference Idx Information */
            pi1_ref_idx[u4_i] = (WORD8)u4_ref_idx;
        }
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_parse_bmb_ref_index_cavlc                                */
/*                                                                           */
/*  Description   : This function does the Cavlc  TEV range > 1 parsing of   */
/*                  reference index  for a B MB.                             */
/*                  Range > 1 when num_ref_idx_active_minus1 > 0             */
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
/*         19 09 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_parse_bmb_ref_index_cavlc(UWORD32 u4_num_part, /* Number of partitions in MB      */
                                      dec_bit_stream_t *ps_bitstrm, /* Pointer to bitstream Structure. */
                                      WORD8 *pi1_ref_idx, /* pointer to reference index array */
                                      UWORD32 u4_num_ref_idx_active_minus1 /* Number of active references - 1  */
                                      )
{
    UWORD32 u4_i;
    UWORD32 *pu4_bitstrm_buf = ps_bitstrm->pu4_buffer;
    UWORD32 *pu4_bitstream_off = &ps_bitstrm->u4_ofst;

    for(u4_i = 0; u4_i < u4_num_part; u4_i++)
    {
        if(pi1_ref_idx[u4_i] > -1)
        {
            UWORD32 u4_ref_idx;
//inlining ih264d_uev
            UWORD32 u4_bitstream_offset = *pu4_bitstream_off;
            UWORD32 u4_word, u4_ldz;

            /***************************************************************/
            /* Find leading zeros in next 32 bits                          */
            /***************************************************************/
            NEXTBITS_32(u4_word, u4_bitstream_offset, pu4_bitstrm_buf);
            u4_ldz = CLZ(u4_word);
            /* Flush the ps_bitstrm */
            u4_bitstream_offset += (u4_ldz + 1);
            /* Read the suffix from the ps_bitstrm */
            u4_word = 0;
            if(u4_ldz)
                GETBITS(u4_word, u4_bitstream_offset, pu4_bitstrm_buf, u4_ldz);
            *pu4_bitstream_off = u4_bitstream_offset;
            u4_ref_idx = ((1 << u4_ldz) + u4_word - 1);
//inlining ih264d_uev
            if(u4_ref_idx > u4_num_ref_idx_active_minus1)
                return ERROR_REF_IDX;

            /* Storing Reference Idx Information */
            pi1_ref_idx[u4_i] = (WORD8)u4_ref_idx;
        }
    }
    return OK;
}
/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cavlc_parse_8x8block_both_available                      */
/*                                                                           */
/*  Description   : This function does the residual parsing of 4 subblocks   */
/*                  in a 8x8 block when both top and left are available      */
/*                                                                           */
/*  Inputs        : pi2_coeff_block : pointer to residual block where        */
/*                  decoded and inverse scan coefficients are updated        */
/*                                                                           */
/*                  u4_sub_block_strd : indicates the number of sublocks    */
/*                  in a row. It is 4 for luma and 2 for chroma.             */
/*                                                                           */
/*                  u4_isdc : required to indicate 4x4 parse modules if the  */
/*                  current  Mb is I_16x16/chroma DC coded.                  */
/*                                                                           */
/*                  ps_dec : pointer to Decstruct (decoder context)          */
/*                                                                           */
/*                  pu1_top_nnz : top nnz pointer                            */
/*                                                                           */
/*                  pu1_left_nnz : left nnz pointer                          */
/*                                                                           */
/*  Globals       : No                                                       */
/*  Processing    : Parsing for four subblocks in unrolled, top and left nnz */
/*                  are updated on the fly. csbp is set in accordance to     */
/*                  decoded numcoeff for the subblock index in raster order  */
/*                                                                           */
/*  Outputs       : The updated residue buffer, nnzs and csbp current block  */
/*                                                                           */
/*  Returns       : Returns the coded sub block pattern csbp for the block   */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         09 10 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_cavlc_parse_8x8block_both_available(WORD16 *pi2_coeff_block,
                                                  UWORD32 u4_sub_block_strd,
                                                  UWORD32 u4_isdc,
                                                  dec_struct_t * ps_dec,
                                                  UWORD8 *pu1_top_nnz,
                                                  UWORD8 *pu1_left_nnz,
                                                  UWORD8 u1_tran_form8x8,
                                                  UWORD8 u1_mb_field_decodingflag,
                                                  UWORD32 *pu4_csbp)
{
    UWORD32 u4_num_coeff, u4_n, u4_subblock_coded;
    UWORD32 u4_top0, u4_top1;
    UWORD32 *pu4_dummy;
    WORD32 (**pf_cavlc_parse4x4coeff)(WORD16 *pi2_coeff_block,
                                      UWORD32 u4_isdc,
                                      WORD32 u4_n,
                                      struct _DecStruct *ps_dec,
                                      UWORD32 *pu4_dummy) =
                                      ps_dec->pf_cavlc_parse4x4coeff;
    UWORD32 u4_idx = 0;
    UWORD8 *puc_temp;
    WORD32 ret;

    *pu4_csbp = 0;
    /* need to change the inverse scan matrices here */
    puc_temp = ps_dec->pu1_inv_scan;

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 0                    */
    /*------------------------------------------------------*/
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[0];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[0];
        }
    }
    u4_n = (pu1_top_nnz[0] + pu1_left_nnz[0] + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top0 = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 1                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[1];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[1];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = (pu1_top_nnz[1] + u4_num_coeff + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top1 = pu1_left_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 2                    */
    /*------------------------------------------------------*/
    u4_idx += (u4_sub_block_strd - 1);
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[2];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[2];
        }
    }
    else
    {
        pi2_coeff_block += ((u4_sub_block_strd - 1) * NUM_COEFFS_IN_4x4BLK);
    }
    u4_n = (u4_top0 + pu1_left_nnz[1] + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 3                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[3];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[3];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = (u4_top1 + u4_num_coeff + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[1] = pu1_left_nnz[1] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    ps_dec->pu1_inv_scan = puc_temp;

    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cavlc_parse_8x8block_left_available                      */
/*                                                                           */
/*  Description   : This function does the residual parsing of 4 subblocks   */
/*                  in a 8x8 block when only left is available for block     */
/*                                                                           */
/*  Inputs        : pi2_coeff_block : pointer to residual block where        */
/*                  decoded and inverse scan coefficients are updated        */
/*                                                                           */
/*                  u4_sub_block_strd : indicates the number of sublocks    */
/*                  in a row. It is 4 for luma and 2 for chroma.             */
/*                                                                           */
/*                  u4_isdc : required to indicate 4x4 parse modules if the  */
/*                  current  Mb is I_16x16/chroma DC coded.                  */
/*                                                                           */
/*                  ps_dec : pointer to Decstruct (decoder context)          */
/*                                                                           */
/*                  pu1_top_nnz : top nnz pointer                            */
/*                                                                           */
/*                  pu1_left_nnz : left nnz pointer                          */
/*                                                                           */
/*  Globals       : No                                                       */
/*  Processing    : Parsing for four subblocks in unrolled, top and left nnz */
/*                  are updated on the fly. csbp is set in accordance to     */
/*                  decoded numcoeff for the subblock index in raster order  */
/*                                                                           */
/*  Outputs       : The updated residue buffer, nnzs and csbp current block  */
/*                                                                           */
/*  Returns       : Returns the coded sub block pattern csbp for the block   */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         09 10 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_cavlc_parse_8x8block_left_available(WORD16 *pi2_coeff_block,
                                                  UWORD32 u4_sub_block_strd,
                                                  UWORD32 u4_isdc,
                                                  dec_struct_t * ps_dec,
                                                  UWORD8 *pu1_top_nnz,
                                                  UWORD8 *pu1_left_nnz,
                                                  UWORD8 u1_tran_form8x8,
                                                  UWORD8 u1_mb_field_decodingflag,
                                                  UWORD32 *pu4_csbp)
{
    UWORD32 u4_num_coeff, u4_n, u4_subblock_coded;
    UWORD32 u4_top0, u4_top1;
    UWORD32 *pu4_dummy;
    WORD32 (**pf_cavlc_parse4x4coeff)(WORD16 *pi2_coeff_block,
                                      UWORD32 u4_isdc,
                                      WORD32 u4_n,
                                      struct _DecStruct *ps_dec,
                                      UWORD32 *pu4_dummy) =
                                      ps_dec->pf_cavlc_parse4x4coeff;
    UWORD32 u4_idx = 0;
    UWORD8 *puc_temp;
    WORD32 ret;

    *pu4_csbp = 0;
    puc_temp = ps_dec->pu1_inv_scan;

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 0                    */
    /*------------------------------------------------------*/
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[0];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[0];
        }
    }
    u4_n = pu1_left_nnz[0];
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top0 = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 1                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[1];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[1];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = u4_num_coeff;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top1 = pu1_left_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 2                    */
    /*------------------------------------------------------*/
    u4_idx += (u4_sub_block_strd - 1);
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[2];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[2];
        }
    }
    else
    {
        pi2_coeff_block += ((u4_sub_block_strd - 1) * NUM_COEFFS_IN_4x4BLK);
    }
    u4_n = (u4_top0 + pu1_left_nnz[1] + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 3                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[3];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[3];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = (u4_top1 + u4_num_coeff + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[1] = pu1_left_nnz[1] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    ps_dec->pu1_inv_scan = puc_temp;

    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cavlc_parse_8x8block_top_available                       */
/*                                                                           */
/*  Description   : This function does the residual parsing of 4 subblocks   */
/*                  in a 8x8 block when only top is available for block      */
/*                                                                           */
/*  Inputs        : pi2_coeff_block : pointer to residual block where        */
/*                  decoded and inverse scan coefficients are updated        */
/*                                                                           */
/*                  u4_sub_block_strd : indicates the number of sublocks    */
/*                  in a row. It is 4 for luma and 2 for chroma.             */
/*                                                                           */
/*                  u4_isdc : required to indicate 4x4 parse modules if the  */
/*                  current  Mb is I_16x16/chroma DC coded.                  */
/*                                                                           */
/*                  ps_dec : pointer to Decstruct (decoder context)          */
/*                                                                           */
/*                  pu1_top_nnz : top nnz pointer                            */
/*                                                                           */
/*                  pu1_left_nnz : left nnz pointer                          */
/*                                                                           */
/*  Globals       : No                                                       */
/*  Processing    : Parsing for four subblocks in unrolled, top and left nnz */
/*                  are updated on the fly. csbp is set in accordance to     */
/*                  decoded numcoeff for the subblock index in raster order  */
/*                                                                           */
/*  Outputs       : The updated residue buffer, nnzs and csbp current block  */
/*                                                                           */
/*  Returns       : Returns the coded sub block pattern csbp for the block   */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         09 10 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_cavlc_parse_8x8block_top_available(WORD16 *pi2_coeff_block,
                                                 UWORD32 u4_sub_block_strd,
                                                 UWORD32 u4_isdc,
                                                 dec_struct_t * ps_dec,
                                                 UWORD8 *pu1_top_nnz,
                                                 UWORD8 *pu1_left_nnz,
                                                 UWORD8 u1_tran_form8x8,
                                                 UWORD8 u1_mb_field_decodingflag,
                                                 UWORD32 *pu4_csbp)
{
    UWORD32 u4_num_coeff, u4_n, u4_subblock_coded;
    UWORD32 u4_top0, u4_top1;
    UWORD32 *pu4_dummy;
    WORD32 (**pf_cavlc_parse4x4coeff)(WORD16 *pi2_coeff_block,
                                      UWORD32 u4_isdc,
                                      WORD32 u4_n,
                                      struct _DecStruct *ps_dec,
                                      UWORD32 *pu4_dummy) =
                                      ps_dec->pf_cavlc_parse4x4coeff;
    UWORD32 u4_idx = 0;
    UWORD8 *puc_temp;
    WORD32 ret;

    *pu4_csbp = 0;
    puc_temp = ps_dec->pu1_inv_scan;

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 0                    */
    /*------------------------------------------------------*/
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[0];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[0];
        }
    }
    u4_n = pu1_top_nnz[0];
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top0 = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 1                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[1];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[1];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = (pu1_top_nnz[1] + u4_num_coeff + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top1 = pu1_left_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 2                    */
    /*------------------------------------------------------*/
    u4_idx += (u4_sub_block_strd - 1);
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[2];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[2];
        }
    }
    else
    {
        pi2_coeff_block += ((u4_sub_block_strd - 1) * NUM_COEFFS_IN_4x4BLK);
    }
    u4_n = u4_top0;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 3                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[3];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[3];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = (u4_top1 + u4_num_coeff + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[1] = pu1_left_nnz[1] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    ps_dec->pu1_inv_scan = puc_temp;

    return OK;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_cavlc_parse_8x8block_none_available                      */
/*                                                                           */
/*  Description   : This function does the residual parsing of 4 subblocks   */
/*                  in a 8x8 block when none of the neigbours are available  */
/*                                                                           */
/*  Inputs        : pi2_coeff_block : pointer to residual block where        */
/*                  decoded and inverse scan coefficients are updated        */
/*                                                                           */
/*                  u4_sub_block_strd : indicates the number of sublocks    */
/*                  in a row. It is 4 for luma and 2 for chroma.             */
/*                                                                           */
/*                  u4_isdc : required to indicate 4x4 parse modules if the  */
/*                  current  Mb is I_16x16/chroma DC coded.                  */
/*                                                                           */
/*                  ps_dec : pointer to Decstruct (decoder context)          */
/*                                                                           */
/*                  pu1_top_nnz : top nnz pointer                            */
/*                                                                           */
/*                  pu1_left_nnz : left nnz pointer                          */
/*                                                                           */
/*  Globals       : No                                                       */
/*  Processing    : Parsing for four subblocks in unrolled, top and left nnz */
/*                  are updated on the fly. csbp is set in accordance to     */
/*                  decoded numcoeff for the subblock index in raster order  */
/*                                                                           */
/*  Outputs       : The updated residue buffer, nnzs and csbp current block  */
/*                                                                           */
/*  Returns       : Returns the coded sub block pattern csbp for the block   */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         09 10 2008   Jay          Draft                                   */
/*                                                                           */
/*****************************************************************************/
WORD32 ih264d_cavlc_parse_8x8block_none_available(WORD16 *pi2_coeff_block,
                                                  UWORD32 u4_sub_block_strd,
                                                  UWORD32 u4_isdc,
                                                  dec_struct_t * ps_dec,
                                                  UWORD8 *pu1_top_nnz,
                                                  UWORD8 *pu1_left_nnz,
                                                  UWORD8 u1_tran_form8x8,
                                                  UWORD8 u1_mb_field_decodingflag,
                                                  UWORD32 *pu4_csbp)
{
    UWORD32 u4_num_coeff, u4_n, u4_subblock_coded;
    UWORD32 u4_top0, u4_top1;
    UWORD32 *pu4_dummy;
    WORD32 (**pf_cavlc_parse4x4coeff)(WORD16 *pi2_coeff_block,
                                      UWORD32 u4_isdc,
                                      WORD32 u4_n,
                                      struct _DecStruct *ps_dec,
                                      UWORD32 *pu4_dummy) =
                                      ps_dec->pf_cavlc_parse4x4coeff;
    UWORD32 u4_idx = 0;
    UWORD8 *puc_temp;
    WORD32 ret;

    *pu4_csbp = 0;
    puc_temp = ps_dec->pu1_inv_scan;

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 0                    */
    /*------------------------------------------------------*/
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[0];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[0];
        }
    }
    ret = pf_cavlc_parse4x4coeff[0](pi2_coeff_block, u4_isdc, 0,
                                    ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top0 = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 1                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[1];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[1];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = u4_num_coeff;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    u4_top1 = pu1_left_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 2                    */
    /*------------------------------------------------------*/
    u4_idx += (u4_sub_block_strd - 1);
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[2];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[2];
        }
    }
    else
    {
        pi2_coeff_block += ((u4_sub_block_strd - 1) * NUM_COEFFS_IN_4x4BLK);
    }
    u4_n = u4_top0;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[0] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    /*------------------------------------------------------*/
    /* Residual 4x4 decoding: SubBlock 3                    */
    /*------------------------------------------------------*/
    u4_idx++;
    if(u1_tran_form8x8)
    {
        if(!u1_mb_field_decodingflag)
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_prog8x8_cavlc[3];
        }
        else
        {
            ps_dec->pu1_inv_scan =
                            (UWORD8*)gau1_ih264d_inv_scan_int8x8_cavlc[3];
        }
    }
    else
    {
        pi2_coeff_block += NUM_COEFFS_IN_4x4BLK;
    }
    u4_n = (u4_top1 + u4_num_coeff + 1) >> 1;
    ret = pf_cavlc_parse4x4coeff[(u4_n > 7)](pi2_coeff_block, u4_isdc,
                                             u4_n, ps_dec, &u4_num_coeff);
    if(ret != OK)
        return ret;

    pu1_top_nnz[1] = pu1_left_nnz[1] = u4_num_coeff;
    u4_subblock_coded = (u4_num_coeff != 0);
    INSERT_BIT(*pu4_csbp, u4_idx, u4_subblock_coded);

    ps_dec->pu1_inv_scan = puc_temp;

    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_parse_residual4x4_cavlc \endif
 *
 * \brief
 *    This function parses CAVLC syntax of a Luma and Chroma AC Residuals.
 *
 * \return
 *    0 on Success and Error code otherwise
 **************************************************************************
 */

WORD32 ih264d_parse_residual4x4_cavlc(dec_struct_t * ps_dec,
                                      dec_mb_info_t *ps_cur_mb_info,
                                      UWORD8 u1_offset)
{
    UWORD8 u1_cbp = ps_cur_mb_info->u1_cbp;
    UWORD16 ui16_csbp = 0;
    UWORD32 u4_nbr_avl;
    WORD16 *pi2_residual_buf;

    UWORD8 u1_is_top_mb_avail;
    UWORD8 u1_is_left_mb_avail;

    UWORD8 *pu1_top_nnz = ps_cur_mb_info->ps_curmb->pu1_nnz_y;
    UWORD8 *pu1_left_nnz = ps_dec->pu1_left_nnz_y;
    WORD16 *pi2_coeff_block = NULL;
    UWORD32 *pu4_dummy;
    WORD32 ret;

    WORD32 (**pf_cavlc_parse_8x8block)(WORD16 *pi2_coeff_block,
                                       UWORD32 u4_sub_block_strd,
                                       UWORD32 u4_isdc,
                                       struct _DecStruct *ps_dec,
                                       UWORD8 *pu1_top_nnz,
                                       UWORD8 *pu1_left_nnz,
                                       UWORD8 u1_tran_form8x8,
                                       UWORD8 u1_mb_field_decodingflag,
                                       UWORD32 *pu4_dummy) = ps_dec->pf_cavlc_parse_8x8block;


    {
        UWORD8 uc_temp = ps_dec->u1_mb_ngbr_availablity;
        u1_is_top_mb_avail = BOOLEAN(uc_temp & TOP_MB_AVAILABLE_MASK);
        u1_is_left_mb_avail = BOOLEAN(uc_temp & LEFT_MB_AVAILABLE_MASK);
        u4_nbr_avl = (u1_is_top_mb_avail << 1) | u1_is_left_mb_avail;
    }

    ps_cur_mb_info->u1_qp_div6 = ps_dec->u1_qp_y_div6;
    ps_cur_mb_info->u1_qp_rem6 = ps_dec->u1_qp_y_rem6;
    ps_cur_mb_info->u1_qpc_div6 = ps_dec->u1_qp_u_div6;
    ps_cur_mb_info->u1_qpc_rem6 = ps_dec->u1_qp_u_rem6;
    ps_cur_mb_info->u1_qpcr_div6 = ps_dec->u1_qp_v_div6;
    ps_cur_mb_info->u1_qpcr_rem6 = ps_dec->u1_qp_v_rem6;

    if(u1_cbp & 0xf)
    {
        pu1_top_nnz[0] = ps_cur_mb_info->ps_top_mb->pu1_nnz_y[0];
        pu1_top_nnz[1] = ps_cur_mb_info->ps_top_mb->pu1_nnz_y[1];
        pu1_top_nnz[2] = ps_cur_mb_info->ps_top_mb->pu1_nnz_y[2];
        pu1_top_nnz[3] = ps_cur_mb_info->ps_top_mb->pu1_nnz_y[3];

        /*******************************************************************/
        /* Block 0 residual decoding, check cbp and proceed (subblock = 0) */
        /*******************************************************************/
        if(!(u1_cbp & 0x1))
        {
            *(UWORD16 *)(pu1_top_nnz) = 0;
            *(UWORD16 *)(pu1_left_nnz) = 0;

        }
        else
        {
            UWORD32 u4_temp;
            ret = pf_cavlc_parse_8x8block[u4_nbr_avl](
                        pi2_coeff_block, 4, u1_offset, ps_dec, pu1_top_nnz,
                        pu1_left_nnz, ps_cur_mb_info->u1_tran_form8x8,
                        ps_cur_mb_info->u1_mb_field_decodingflag, &u4_temp);
            if(ret != OK)
                return ret;
            ui16_csbp = u4_temp;
        }

        /*******************************************************************/
        /* Block 1 residual decoding, check cbp and proceed (subblock = 2) */
        /*******************************************************************/
        if(ps_cur_mb_info->u1_tran_form8x8)
        {
            pi2_coeff_block += 64;
        }
        else
        {
            pi2_coeff_block += (2 * NUM_COEFFS_IN_4x4BLK);
        }

        if(!(u1_cbp & 0x2))
        {
            *(UWORD16 *)(pu1_top_nnz + 2) = 0;
            *(UWORD16 *)(pu1_left_nnz) = 0;
        }
        else
        {
            UWORD32 u4_temp = (u4_nbr_avl | 0x1);
            ret = pf_cavlc_parse_8x8block[u4_temp](
                        pi2_coeff_block, 4, u1_offset, ps_dec,
                        (pu1_top_nnz + 2), pu1_left_nnz,
                        ps_cur_mb_info->u1_tran_form8x8,
                        ps_cur_mb_info->u1_mb_field_decodingflag, &u4_temp);
            if(ret != OK)
                return ret;
            ui16_csbp |= (u4_temp << 2);
        }

        /*******************************************************************/
        /* Block 2 residual decoding, check cbp and proceed (subblock = 8) */
        /*******************************************************************/
        if(ps_cur_mb_info->u1_tran_form8x8)
        {
            pi2_coeff_block += 64;
        }
        else
        {
            pi2_coeff_block += (6 * NUM_COEFFS_IN_4x4BLK);
        }

        if(!(u1_cbp & 0x4))
        {
            *(UWORD16 *)(pu1_top_nnz) = 0;
            *(UWORD16 *)(pu1_left_nnz + 2) = 0;
        }
        else
        {
            UWORD32 u4_temp = (u4_nbr_avl | 0x2);
            ret = pf_cavlc_parse_8x8block[u4_temp](
                        pi2_coeff_block, 4, u1_offset, ps_dec, pu1_top_nnz,
                        (pu1_left_nnz + 2), ps_cur_mb_info->u1_tran_form8x8,
                        ps_cur_mb_info->u1_mb_field_decodingflag, &u4_temp);
            if(ret != OK)
                return ret;
            ui16_csbp |= (u4_temp << 8);
        }

        /*******************************************************************/
        /* Block 3 residual decoding, check cbp and proceed (subblock = 10)*/
        /*******************************************************************/
        if(ps_cur_mb_info->u1_tran_form8x8)
        {
            pi2_coeff_block += 64;
        }
        else
        {
            pi2_coeff_block += (2 * NUM_COEFFS_IN_4x4BLK);
        }

        if(!(u1_cbp & 0x8))
        {
            *(UWORD16 *)(pu1_top_nnz + 2) = 0;
            *(UWORD16 *)(pu1_left_nnz + 2) = 0;
        }
        else
        {
            UWORD32 u4_temp;
            ret = pf_cavlc_parse_8x8block[0x3](
                        pi2_coeff_block, 4, u1_offset, ps_dec,
                        (pu1_top_nnz + 2), (pu1_left_nnz + 2),
                        ps_cur_mb_info->u1_tran_form8x8,
                        ps_cur_mb_info->u1_mb_field_decodingflag, &u4_temp);
            if(ret != OK)
                return ret;
            ui16_csbp |= (u4_temp << 10);
        }
    }
    else
    {
        *(UWORD32 *)(pu1_top_nnz) = 0;
        *(UWORD32 *)(pu1_left_nnz) = 0;
    }

    ps_cur_mb_info->u2_luma_csbp = ui16_csbp;
    ps_cur_mb_info->ps_curmb->u2_luma_csbp = ui16_csbp;

    {
        UWORD16 u2_chroma_csbp = 0;
        ps_cur_mb_info->u2_chroma_csbp = 0;
        pu1_top_nnz = ps_cur_mb_info->ps_curmb->pu1_nnz_uv;
        pu1_left_nnz = ps_dec->pu1_left_nnz_uv;

        u1_cbp >>= 4;
        /*--------------------------------------------------------------------*/
        /* if Chroma Component not present OR no ac values present            */
        /* Set the values of N to zero                                        */
        /*--------------------------------------------------------------------*/
        if(u1_cbp == CBPC_ALLZERO || u1_cbp == CBPC_ACZERO)
        {
            *(UWORD32 *)(pu1_top_nnz) = 0;
            *(UWORD32 *)(pu1_left_nnz) = 0;
        }

        if(u1_cbp == CBPC_ALLZERO)
        {
            return (0);
        }
        /*--------------------------------------------------------------------*/
        /* Decode Chroma DC values                                            */
        /*--------------------------------------------------------------------*/
        {
            WORD32 u4_scale_u;
            WORD32 u4_scale_v;
            WORD32 i4_mb_inter_inc;
            u4_scale_u = ps_dec->pu2_quant_scale_u[0] << ps_dec->u1_qp_u_div6;
            u4_scale_v = ps_dec->pu2_quant_scale_v[0] << ps_dec->u1_qp_v_div6;
            i4_mb_inter_inc = (!((ps_cur_mb_info->ps_curmb->u1_mb_type == I_4x4_MB)
                            || (ps_cur_mb_info->ps_curmb->u1_mb_type == I_16x16_MB)))
                            * 3;

            if(ps_dec->s_high_profile.u1_scaling_present)
            {
                u4_scale_u *=
                                ps_dec->s_high_profile.i2_scalinglist4x4[i4_mb_inter_inc
                                                + 1][0];
                u4_scale_v *=
                                ps_dec->s_high_profile.i2_scalinglist4x4[i4_mb_inter_inc
                                                + 2][0];

            }
            else
            {
                u4_scale_u <<= 4;
                u4_scale_v <<= 4;
            }

            ih264d_cavlc_parse_chroma_dc(ps_cur_mb_info,pi2_coeff_block, ps_dec->ps_bitstrm,
                                         u4_scale_u, u4_scale_v,
                                         i4_mb_inter_inc);
        }

        if(u1_cbp == CBPC_ACZERO)
            return (0);

        pu1_top_nnz[0] = ps_cur_mb_info->ps_top_mb->pu1_nnz_uv[0];
        pu1_top_nnz[1] = ps_cur_mb_info->ps_top_mb->pu1_nnz_uv[1];
        pu1_top_nnz[2] = ps_cur_mb_info->ps_top_mb->pu1_nnz_uv[2];
        pu1_top_nnz[3] = ps_cur_mb_info->ps_top_mb->pu1_nnz_uv[3];
        /*--------------------------------------------------------------------*/
        /* Decode Chroma AC values                                            */
        /*--------------------------------------------------------------------*/
        {
            UWORD32 u4_temp;
            /*****************************************************************/
            /* U Block  residual decoding, check cbp and proceed (subblock=0)*/
            /*****************************************************************/
            ret = pf_cavlc_parse_8x8block[u4_nbr_avl](
                        pi2_coeff_block, 2, 1, ps_dec, pu1_top_nnz,
                        pu1_left_nnz, 0, 0, &u4_temp);
            if(ret != OK)
                return ret;
            u2_chroma_csbp = u4_temp;

            pi2_coeff_block += MB_CHROM_SIZE;
            /*****************************************************************/
            /* V Block  residual decoding, check cbp and proceed (subblock=1)*/
            /*****************************************************************/
            ret = pf_cavlc_parse_8x8block[u4_nbr_avl](pi2_coeff_block, 2, 1,
                                                      ps_dec,
                                                      (pu1_top_nnz + 2),
                                                      (pu1_left_nnz + 2), 0,
                                                      0, &u4_temp);
            if(ret != OK)
                return ret;
            u2_chroma_csbp |= (u4_temp << 4);
        }

        ps_cur_mb_info->u2_chroma_csbp = u2_chroma_csbp;
    }
    return OK;
}
