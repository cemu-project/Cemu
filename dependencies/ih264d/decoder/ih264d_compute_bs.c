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

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_deblocking.h"
#include "string.h"
#include "ih264d_debug.h"
#include "ih264d_tables.h"

UWORD16 ih264d_update_csbp_8x8(UWORD16 u2_luma_csbp)
{
    UWORD16 u2_mod_csbp;

    u2_mod_csbp = u2_luma_csbp;

    if(u2_mod_csbp & 0x0033)
    {
        u2_mod_csbp |= 0x0033;
    }

    if(u2_mod_csbp & 0x00CC)
    {
        u2_mod_csbp |= 0x00CC;
    }

    if(u2_mod_csbp & 0x3300)
    {
        u2_mod_csbp |= 0x3300;
    }

    if(u2_mod_csbp & 0xCC00)
    {
        u2_mod_csbp |= 0xCC00;
    }

    return u2_mod_csbp;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs2_horz_vert                                       */
/*                                                                           */
/*  Description   : This function fills boundray strength (=2) for all horz  */
/*                  and vert edges of current mb based on coded sub block    */
/*                  pattern of current, top and left mb                      */
/*  Inputs        :                                                          */
/*                  pu4_bs : Base pointer of  BS table which gets updated    */
/*                  u4_left_mb_csbp : left mb's coded sub block pattern      */
/*                  u4_top_mb_csbp : top mb's coded sub block pattern        */
/*                  u4_cur_mb_csbp : current mb's coded sub block pattern    */
/*                                                                           */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    :                                                          */
/*                                                                           */
/*              csbp for each 4x4 block in a mb is bit packet in reverse     */
/*              raster scan order for each mb as shown below:                */
/*              15|14|13|12|11|10|9|8|7|6|5|4|3|2|1|0.                       */
/*                                                                           */
/*              BS=2 for a 4x4 edge if any of adjacent blocks forming edge   */
/*              are coded. Keeping this in mind, bs=2 for all horz and vert  */
/*              edges can be derived using  a lookup table for each edge     */
/*              after "ORing" the csbp values as follows:                    */
/*              (C means current Mb, T means top mb and L means left mb)     */
/*                                                                           */
/*              All Horz edges:                                              */
/*              15C|14C|13C|12C|11C|10C|9C|8C|7C|6C|5C|4C|3C |2C |1C |0C     */
/*  (or with)   11C|10C| 9C| 8C| 7C|6C |5C|4C|3C|2C|1C|0C|15T|14T|13T|12T    */
/*              -----BS[3]-----|----BS[2]----|---BS[1]---|----BS[0]-----|    */
/*                                                                           */
/*              All Vert edges:                                              */
/*              15C|14C|13C|12C|11C|10C|9C| 8C|7C|6C|5C|4C|3C |2C |1C |0C    */
/*  (or with)   14C|13C|12C|15L|10C| 9C|8C|11L|6C|5C|4C|7L|2C |1C |0C |3L    */
/*              Do 4x4 transpose of resulting pattern to get vertBS[4]-BS[7] */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
#define CSBP_LEFT_BLOCK_MASK 0x1111
#define CSBP_RIGHT_BLOCK_MASK 0x8888

void ih264d_fill_bs2_horz_vert(UWORD32 *pu4_bs, /* Base pointer of BS table */
                               WORD32 u4_left_mb_csbp, /* csbp of left mb */
                               WORD32 u4_top_mb_csbp, /* csbp of top mb */
                               WORD32 u4_cur_mb_csbp, /* csbp of current mb */
                               const UWORD32 *pu4_packed_bs2, const UWORD16 *pu2_4x4_v2h_reorder)
{
    /*************************************************************************/
    /*u4_nbr_horz_csbp=11C|10C|9C|8C|7C|6C|5C|4C|3C|2C|1C|0C|15T|14T|13T|12T */
    /*************************************************************************/
    UWORD32 u4_nbr_horz_csbp = (u4_cur_mb_csbp << 4) | (u4_top_mb_csbp >> 12);
    UWORD32 u4_horz_bs2_dec = u4_cur_mb_csbp | u4_nbr_horz_csbp;

    /*************************************************************************/
    /*u4_left_mb_masked_csbp = 15L|0|0|0|11L|0|0|0|7L|0|0|0|3L|0|0|0         */
    /*************************************************************************/
    UWORD32 u4_left_mb_masked_csbp = u4_left_mb_csbp & CSBP_RIGHT_BLOCK_MASK;

    /*************************************************************************/
    /*u4_cur_mb_masked_csbp =14C|13C|12C|x|10C|9C|8C|x|6C|5C|4C|x|2C|1C|0C|x */
    /*************************************************************************/
    UWORD32 u4_cur_mb_masked_csbp = (u4_cur_mb_csbp << 1)
                    & (~CSBP_LEFT_BLOCK_MASK);

    /*************************************************************************/
    /*u4_nbr_vert_csbp=14C|13C|12C|15L|10C|9C|8C|11L|6C|5C|4C|7L|2C|1C|0C|3L */
    /*************************************************************************/
    UWORD32 u4_nbr_vert_csbp = (u4_cur_mb_masked_csbp)
                    | (u4_left_mb_masked_csbp >> 3);

    UWORD32 u4_vert_bs2_dec = u4_cur_mb_csbp | u4_nbr_vert_csbp;

    UWORD32 u4_reordered_vert_bs2_dec, u4_temp;

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    /*************************************************************************/
    /* Fill horz edges (0,1,2,3) boundary strengths 2 using look up table    */
    /*************************************************************************/
    pu4_bs[0] = pu4_packed_bs2[u4_horz_bs2_dec & 0xF];
    pu4_bs[1] = pu4_packed_bs2[(u4_horz_bs2_dec >> 4) & 0xF];
    pu4_bs[2] = pu4_packed_bs2[(u4_horz_bs2_dec >> 8) & 0xF];
    pu4_bs[3] = pu4_packed_bs2[(u4_horz_bs2_dec >> 12) & 0xF];

    /*************************************************************************/
    /* Do 4x4 tranpose of u4_vert_bs2_dec by using look up table for reorder */
    /*************************************************************************/
    u4_reordered_vert_bs2_dec = pu2_4x4_v2h_reorder[u4_vert_bs2_dec & 0xF];
    u4_temp = pu2_4x4_v2h_reorder[(u4_vert_bs2_dec >> 4) & 0xF];
    u4_reordered_vert_bs2_dec |= (u4_temp << 1);
    u4_temp = pu2_4x4_v2h_reorder[(u4_vert_bs2_dec >> 8) & 0xF];
    u4_reordered_vert_bs2_dec |= (u4_temp << 2);
    u4_temp = pu2_4x4_v2h_reorder[(u4_vert_bs2_dec >> 12) & 0xF];
    u4_reordered_vert_bs2_dec |= (u4_temp << 3);

    /*************************************************************************/
    /* Fill vert edges (4,5,6,7) boundary strengths 2 using look up table    */
    /*************************************************************************/
    pu4_bs[4] = pu4_packed_bs2[u4_reordered_vert_bs2_dec & 0xF];
    pu4_bs[5] = pu4_packed_bs2[(u4_reordered_vert_bs2_dec >> 4) & 0xF];
    pu4_bs[6] = pu4_packed_bs2[(u4_reordered_vert_bs2_dec >> 8) & 0xF];
    pu4_bs[7] = pu4_packed_bs2[(u4_reordered_vert_bs2_dec >> 12) & 0xF];
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs1_16x16mb_pslice                                  */
/*                                                                           */
/*  Description   : This function fills boundray strength (=1) for those     */
/*                  horz and vert mb edges of 16x16mb which are set to 0 by  */
/*                  ih264d_fill_bs2_horz_vert. This function is used for p slices   */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : If any motion vector component of adjacent 4x4 blocks    */
/*                  differs by more than 1 integer pel or if reference       */
/*                  pictures are different, Bs is set to 1.                  */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_fill_bs1_16x16mb_pslice(mv_pred_t *ps_cur_mv_pred,
                                    mv_pred_t *ps_top_mv_pred,
                                    void **ppv_map_ref_idx_to_poc,
                                    UWORD32 *pu4_bs_table, /* pointer to the BsTable array */
                                    mv_pred_t *ps_leftmost_mv_pred,
                                    neighbouradd_t *ps_left_addr,
                                    void **u4_pic_addrress, /* picture address for BS calc */
                                    WORD32 i4_ver_mvlimit)
{
    WORD16 i2_q_mv0, i2_q_mv1;
    WORD16 i2_p_mv0, i2_p_mv1;
    void *pv_cur_pic_addr0, *pv_cur_pic_addr1;
    void *pv_nbr_pic_addr0, *pv_nbr_pic_addr1;
    void **ppv_map_ref_idx_to_poc_l0; //,*ppv_map_ref_idx_to_poc_l1;
    UWORD32 i;
    UWORD32 u4_bs_horz = pu4_bs_table[0];
    UWORD32 u4_bs_vert = pu4_bs_table[4];

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    ppv_map_ref_idx_to_poc_l0 = ppv_map_ref_idx_to_poc;

    i2_q_mv0 = ps_cur_mv_pred->i2_mv[0];
    i2_q_mv1 = ps_cur_mv_pred->i2_mv[1];
    pv_cur_pic_addr0 = ppv_map_ref_idx_to_poc_l0[ps_cur_mv_pred->i1_ref_frame[0]];
    pv_cur_pic_addr1 = 0;

    /*********************************/
    /* Computing Bs for the top edge */
    /*********************************/
    for(i = 0; i < 4; i++, ps_top_mv_pred++)
    {
        UWORD32 u4_idx = 24 - (i << 3);

        /*********************************/
        /* check if Bs is already set    */
        /*********************************/
        if(!((u4_bs_horz >> u4_idx) & 0xf))
        {
            /************************************************************/
            /* If Bs is not set, use left edge and current edge mvs and */
            /* reference pictures addresses to evaluate Bs==1           */
            /************************************************************/
            UWORD32 u4_bs_temp1;
            UWORD32 u4_bs;

            /*********************************************************/
            /* If any motion vector component differs by more than 1 */
            /* integer pel or if reference pictures are different Bs */
            /* is set to 1. Note that this condition shall be met for*/
            /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
            /*********************************************************/
            i2_p_mv0 = ps_top_mv_pred->i2_mv[0];
            i2_p_mv1 = ps_top_mv_pred->i2_mv[1];
            pv_nbr_pic_addr0 = u4_pic_addrress[i & 2];
            pv_nbr_pic_addr1 = u4_pic_addrress[1 + (i & 2)];

            u4_bs_temp1 = ((ABS((i2_p_mv0 - i2_q_mv0)) >= 4) ||
                           (ABS((i2_p_mv1 - i2_q_mv1)) >= i4_ver_mvlimit));

            u4_bs = ((pv_cur_pic_addr0 != pv_nbr_pic_addr0)
                            || (pv_cur_pic_addr1 != pv_nbr_pic_addr1)
                            || u4_bs_temp1);

            u4_bs_horz |= (u4_bs << u4_idx);
        }
    }
    pu4_bs_table[0] = u4_bs_horz;

    /***********************************/
    /* Computing Bs for the left edge  */
    /***********************************/
    for(i = 0; i < 4; i++, ps_leftmost_mv_pred += 4)
    {
        UWORD32 u4_idx = 24 - (i << 3);

        /*********************************/
        /* check if Bs is already set    */
        /*********************************/
        if(!((u4_bs_vert >> u4_idx) & 0xf))
        {
            /****************************************************/
            /* If Bs is not set, evalaute conditions for Bs=1   */
            /****************************************************/
            UWORD32 u4_bs_temp1;
            UWORD32 u4_bs;
            /*********************************************************/
            /* If any motion vector component differs by more than 1 */
            /* integer pel or if reference pictures are different Bs */
            /* is set to 1. Note that this condition shall be met for*/
            /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
            /*********************************************************/

            i2_p_mv0 = ps_leftmost_mv_pred->i2_mv[0];
            i2_p_mv1 = ps_leftmost_mv_pred->i2_mv[1];
            pv_nbr_pic_addr0 = ps_left_addr->u4_add[i & 2];
            pv_nbr_pic_addr1 = ps_left_addr->u4_add[1 + (i & 2)];

            u4_bs_temp1 =
                            ((ABS((i2_p_mv0 - i2_q_mv0))
                                            >= 4)
                                            | (ABS((i2_p_mv1 - i2_q_mv1))
                                                            >= i4_ver_mvlimit));

            u4_bs = ((pv_cur_pic_addr0 != pv_nbr_pic_addr0)
                            || (pv_cur_pic_addr1 != pv_nbr_pic_addr1)
                            || u4_bs_temp1);

            u4_bs_vert |= (u4_bs << u4_idx);
        }
    }
    pu4_bs_table[4] = u4_bs_vert;

    return;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs1_non16x16mb_pslice                               */
/*                                                                           */
/*  Description   : This function fills boundray strength (=1) for those     */
/*                  horz and vert edges of non16x16mb which are set to 0 by  */
/*                  ih264d_fill_bs2_horz_vert. This function is used for p slices   */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : If any motion vector component of adjacent 4x4 blocks    */
/*                  differs by more than 1 integer pel or if reference       */
/*                  pictures are different, Bs is set to 1.                  */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_fill_bs1_non16x16mb_pslice(mv_pred_t *ps_cur_mv_pred,
                                       mv_pred_t *ps_top_mv_pred,
                                       void **ppv_map_ref_idx_to_poc,
                                       UWORD32 *pu4_bs_table, /* pointer to the BsTable array */
                                       mv_pred_t *ps_leftmost_mv_pred,
                                       neighbouradd_t *ps_left_addr,
                                       void **u4_pic_addrress,
                                       WORD32 i4_ver_mvlimit)
{
    UWORD32 edge;
    void **ppv_map_ref_idx_to_poc_l0; //,*ppv_map_ref_idx_to_poc_l1;

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    ppv_map_ref_idx_to_poc_l0 = ppv_map_ref_idx_to_poc;


    for(edge = 0; edge < 4; edge++, ps_top_mv_pred = ps_cur_mv_pred - 4)
    {
        /*********************************************************************/
        /* Each iteration of this loop fills the four BS values of one HORIZ */
        /* edge and one BS value for each of the four VERT edges.            */
        /*********************************************************************/
        WORD32 i;
        UWORD32 u4_vert_idx = 24 - (edge << 3);
        UWORD32 u4_bs_horz = pu4_bs_table[edge];
        mv_pred_t *ps_left_mv_pred = ps_leftmost_mv_pred + (edge << 2);

        for(i = 0; i < 4; i++, ps_top_mv_pred++, ps_cur_mv_pred++)
        {
            WORD16 i2_cur_mv0, i2_cur_mv1;
            WORD8 i1_cur_ref0;
            void *pv_cur_pic_addr0, *pv_cur_pic_addr1 = 0;
            void *pv_nbr_pic_addr0, *pv_nbr_pic_addr1;

            /******************************************************/
            /* Each iteration of this inner loop computes a HORIZ */
            /* and a VERT BS value for a 4x4 block                */
            /******************************************************/
            UWORD32 u4_bs_vert = (pu4_bs_table[i + 4] >> u4_vert_idx) & 0xf;
            UWORD32 u4_horz_idx = 24 - (i << 3);

            /*****************************************************/
            /* check if vert Bs for this block is already set    */
            /*****************************************************/
            if(!u4_bs_vert)
            {
                WORD16 i2_left_mv0, i2_left_mv1;
                /************************************************************/
                /* If Bs is not set, use left edge and current edge mvs and */
                /* reference pictures addresses to evaluate Bs==1           */
                /************************************************************/
                i2_left_mv0 = ps_left_mv_pred->i2_mv[0];
                i2_left_mv1 = ps_left_mv_pred->i2_mv[1];

                i2_cur_mv0 = ps_cur_mv_pred->i2_mv[0];
                i2_cur_mv1 = ps_cur_mv_pred->i2_mv[1];

                i1_cur_ref0 = ps_cur_mv_pred->i1_ref_frame[0];

                pv_cur_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_cur_ref0];
                if(i)
                {
                    WORD8 i1_left_ref0 = ps_left_mv_pred->i1_ref_frame[0];
                    pv_nbr_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_left_ref0];
                    pv_nbr_pic_addr1 = 0;
                }
                else
                {
                    pv_nbr_pic_addr0 = ps_left_addr->u4_add[edge & 2];
                    pv_nbr_pic_addr1 = ps_left_addr->u4_add[1 + (edge & 2)];
                }

                {
                    UWORD32 u4_bs_temp1;
                    /*********************************************************/
                    /* If any motion vector component differs by more than 1 */
                    /* integer pel or if reference pictures are different Bs */
                    /* is set to 1. Note that this condition shall be met for*/
                    /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
                    /*********************************************************/

                    u4_bs_temp1 =
                                    ((ABS((i2_left_mv0 - i2_cur_mv0))
                                                    >= 4)
                                                    | (ABS((i2_left_mv1
                                                                    - i2_cur_mv1))
                                                                    >= i4_ver_mvlimit));

                    u4_bs_vert = ((pv_nbr_pic_addr0 != pv_cur_pic_addr0)
                                    || (pv_nbr_pic_addr1 != pv_cur_pic_addr1)
                                    || u4_bs_temp1);

                    pu4_bs_table[i + 4] |= (u4_bs_vert << u4_vert_idx);
                }
            }

            /*****************************************************/
            /* check if horz Bs for this block is already set    */
            /*****************************************************/
            if(!((u4_bs_horz >> u4_horz_idx) & 0xf))
            {
                WORD16 i2_top_mv0, i2_top_mv1;
                /************************************************************/
                /* If Bs is not set, use top edge and current edge mvs and  */
                /* reference pictures addresses to evaluate Bs==1           */
                /************************************************************/
                i2_cur_mv0 = ps_cur_mv_pred->i2_mv[0];
                i2_cur_mv1 = ps_cur_mv_pred->i2_mv[1];

                i1_cur_ref0 = ps_cur_mv_pred->i1_ref_frame[0];

                i2_top_mv0 = ps_top_mv_pred->i2_mv[0];
                i2_top_mv1 = ps_top_mv_pred->i2_mv[1];

                pv_cur_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_cur_ref0];
                if(edge)
                {
                    WORD8 i1_top_ref0 = ps_top_mv_pred->i1_ref_frame[0];
                    pv_nbr_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_top_ref0];
                    pv_nbr_pic_addr1 = 0;
                }
                else
                {
                    pv_nbr_pic_addr0 = u4_pic_addrress[i & 2];
                    pv_nbr_pic_addr1 = u4_pic_addrress[1 + (i & 2)];
                }

                {
                    UWORD32 u4_bs_temp1;
                    UWORD32 u4_bs;
                    /*********************************************************/
                    /* If any motion vector component differs by more than 1 */
                    /* integer pel or if reference pictures are different Bs */
                    /* is set to 1. Note that this condition shall be met for*/
                    /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
                    /*********************************************************/

                    u4_bs_temp1 =
                                    ((ABS((i2_top_mv0 - i2_cur_mv0))
                                                    >= 4)
                                                    | (ABS((i2_top_mv1
                                                                    - i2_cur_mv1))
                                                                    >= i4_ver_mvlimit));

                    u4_bs = ((pv_nbr_pic_addr0 != pv_cur_pic_addr0)
                                    || (pv_nbr_pic_addr1 != pv_cur_pic_addr1)
                                    || u4_bs_temp1);

                    u4_bs_horz |= (u4_bs << u4_horz_idx);
                }
            }

            ps_left_mv_pred = ps_cur_mv_pred;
        }

        pu4_bs_table[edge] = u4_bs_horz;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs1_16x16mb_bslice                                  */
/*                                                                           */
/*  Description   : This function fills boundray strength (=1) for those     */
/*                  horz and vert mb edges of 16x16mb which are set to 0 by  */
/*                  ih264d_fill_bs2_horz_vert. This function is used for b slices   */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : If any motion vector component of adjacent 4x4 blocks    */
/*                  differs by more than 1 integer pel or if reference       */
/*                  pictures are different, Bs is set to 1.                  */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_fill_bs1_16x16mb_bslice(mv_pred_t *ps_cur_mv_pred,
                                    mv_pred_t *ps_top_mv_pred,
                                    void **ppv_map_ref_idx_to_poc,
                                    UWORD32 *pu4_bs_table, /* pointer to the BsTable array */
                                    mv_pred_t *ps_leftmost_mv_pred,
                                    neighbouradd_t *ps_left_addr,
                                    void **u4_pic_addrress,
                                    WORD32 i4_ver_mvlimit)
{
    WORD16 i2_q_mv0, i2_q_mv1, i2_q_mv2, i2_q_mv3;
    WORD16 i2_p_mv0, i2_p_mv1, i2_p_mv2, i2_p_mv3;
    void *pv_cur_pic_addr0, *pv_cur_pic_addr1;
    void *pv_nbr_pic_addr0, *pv_nbr_pic_addr1;
    void **ppv_map_ref_idx_to_poc_l0, **ppv_map_ref_idx_to_poc_l1;
    UWORD32 i;
    UWORD32 u4_bs_horz = pu4_bs_table[0];
    UWORD32 u4_bs_vert = pu4_bs_table[4];

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    ppv_map_ref_idx_to_poc_l0 = ppv_map_ref_idx_to_poc;
    ppv_map_ref_idx_to_poc_l1 = ppv_map_ref_idx_to_poc + POC_LIST_L0_TO_L1_DIFF;
    i2_q_mv0 = ps_cur_mv_pred->i2_mv[0];
    i2_q_mv1 = ps_cur_mv_pred->i2_mv[1];
    i2_q_mv2 = ps_cur_mv_pred->i2_mv[2];
    i2_q_mv3 = ps_cur_mv_pred->i2_mv[3];
    pv_cur_pic_addr0 = ppv_map_ref_idx_to_poc_l0[ps_cur_mv_pred->i1_ref_frame[0]];
    pv_cur_pic_addr1 = ppv_map_ref_idx_to_poc_l1[ps_cur_mv_pred->i1_ref_frame[1]];

    /*********************************/
    /* Computing Bs for the top edge */
    /*********************************/
    for(i = 0; i < 4; i++, ps_top_mv_pred++)
    {
        UWORD32 u4_idx = 24 - (i << 3);

        /*********************************/
        /* check if Bs is already set    */
        /*********************************/
        if(!((u4_bs_horz >> u4_idx) & 0xf))
        {
            /************************************************************/
            /* If Bs is not set, use left edge and current edge mvs and */
            /* reference pictures addresses to evaluate Bs==1           */
            /************************************************************/
            UWORD32 u4_bs_temp1, u4_bs_temp2;
            UWORD32 u4_bs;

            /*********************************************************/
            /* If any motion vector component differs by more than 1 */
            /* integer pel or if reference pictures are different Bs */
            /* is set to 1. Note that this condition shall be met for*/
            /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
            /*********************************************************/
            i2_p_mv0 = ps_top_mv_pred->i2_mv[0];
            i2_p_mv1 = ps_top_mv_pred->i2_mv[1];
            i2_p_mv2 = ps_top_mv_pred->i2_mv[2];
            i2_p_mv3 = ps_top_mv_pred->i2_mv[3];
            pv_nbr_pic_addr0 = u4_pic_addrress[i & 2];
            pv_nbr_pic_addr1 = u4_pic_addrress[1 + (i & 2)];

            u4_bs_temp1 =
                            ((ABS((i2_p_mv0 - i2_q_mv0))
                                            >= 4)
                                            | (ABS((i2_p_mv1 - i2_q_mv1))
                                                            >= i4_ver_mvlimit)
                                            | (ABS((i2_p_mv2 - i2_q_mv2))
                                                            >= 4)
                                            | (ABS((i2_p_mv3 - i2_q_mv3))
                                                            >= i4_ver_mvlimit));

            u4_bs_temp2 =
                            ((ABS((i2_p_mv0 - i2_q_mv2))
                                            >= 4)
                                            | (ABS((i2_p_mv1 - i2_q_mv3))
                                                            >= i4_ver_mvlimit)
                                            | (ABS((i2_p_mv2 - i2_q_mv0))
                                                            >= 4)
                                            | (ABS((i2_p_mv3 - i2_q_mv1))
                                                            >= i4_ver_mvlimit));

            u4_bs = ((pv_cur_pic_addr0 != pv_nbr_pic_addr0)
                            || (pv_cur_pic_addr1 != pv_nbr_pic_addr1)
                            || u4_bs_temp1)
                            && ((pv_cur_pic_addr0 != pv_nbr_pic_addr1)
                                            || (pv_cur_pic_addr1
                                                            != pv_nbr_pic_addr0)
                                            || u4_bs_temp2);

            u4_bs_horz |= (u4_bs << u4_idx);
        }
    }
    pu4_bs_table[0] = u4_bs_horz;

    /***********************************/
    /* Computing Bs for the left edge  */
    /***********************************/
    for(i = 0; i < 4; i++, ps_leftmost_mv_pred += 4)
    {
        UWORD32 u4_idx = 24 - (i << 3);

        /*********************************/
        /* check if Bs is already set    */
        /*********************************/
        if(!((u4_bs_vert >> u4_idx) & 0xf))
        {
            /****************************************************/
            /* If Bs is not set, evalaute conditions for Bs=1   */
            /****************************************************/
            UWORD32 u4_bs_temp1, u4_bs_temp2;
            UWORD32 u4_bs;
            /*********************************************************/
            /* If any motion vector component differs by more than 1 */
            /* integer pel or if reference pictures are different Bs */
            /* is set to 1. Note that this condition shall be met for*/
            /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
            /*********************************************************/

            i2_p_mv0 = ps_leftmost_mv_pred->i2_mv[0];
            i2_p_mv1 = ps_leftmost_mv_pred->i2_mv[1];
            i2_p_mv2 = ps_leftmost_mv_pred->i2_mv[2];
            i2_p_mv3 = ps_leftmost_mv_pred->i2_mv[3];
            pv_nbr_pic_addr0 = ps_left_addr->u4_add[i & 2];
            pv_nbr_pic_addr1 = ps_left_addr->u4_add[1 + (i & 2)];

            u4_bs_temp1 =
                            ((ABS((i2_p_mv0 - i2_q_mv0))
                                            >= 4)
                                            | (ABS((i2_p_mv1 - i2_q_mv1))
                                                            >= i4_ver_mvlimit)
                                            | (ABS((i2_p_mv2 - i2_q_mv2))
                                                            >= 4)
                                            | (ABS((i2_p_mv3 - i2_q_mv3))
                                                            >= i4_ver_mvlimit));

            u4_bs_temp2 =
                            ((ABS((i2_p_mv0 - i2_q_mv2))
                                            >= 4)
                                            | (ABS((i2_p_mv1 - i2_q_mv3))
                                                            >= i4_ver_mvlimit)
                                            | (ABS((i2_p_mv2 - i2_q_mv0))
                                                            >= 4)
                                            | (ABS((i2_p_mv3 - i2_q_mv1))
                                                            >= i4_ver_mvlimit));

            u4_bs = ((pv_cur_pic_addr0 != pv_nbr_pic_addr0)
                            || (pv_cur_pic_addr1 != pv_nbr_pic_addr1)
                            || u4_bs_temp1)
                            && ((pv_cur_pic_addr0 != pv_nbr_pic_addr1)
                                            || (pv_cur_pic_addr1
                                                            != pv_nbr_pic_addr0)
                                            || u4_bs_temp2);

            u4_bs_vert |= (u4_bs << u4_idx);
        }
    }
    pu4_bs_table[4] = u4_bs_vert;

    return;
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs1_non16x16mb_bslice                               */
/*                                                                           */
/*  Description   : This function fills boundray strength (=1) for those     */
/*                  horz and vert edges of non16x16mb which are set to 0 by  */
/*                  ih264d_fill_bs2_horz_vert. This function is used for b slices   */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    : If any motion vector component of adjacent 4x4 blocks    */
/*                  differs by more than 1 integer pel or if reference       */
/*                  pictures are different, Bs is set to 1.                  */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_fill_bs1_non16x16mb_bslice(mv_pred_t *ps_cur_mv_pred,
                                       mv_pred_t *ps_top_mv_pred,
                                       void **ppv_map_ref_idx_to_poc,
                                       UWORD32 *pu4_bs_table, /* pointer to the BsTable array */
                                       mv_pred_t *ps_leftmost_mv_pred,
                                       neighbouradd_t *ps_left_addr,
                                       void **u4_pic_addrress,
                                       WORD32 i4_ver_mvlimit)
{
    UWORD32 edge;
    void **ppv_map_ref_idx_to_poc_l0, **ppv_map_ref_idx_to_poc_l1;
    ppv_map_ref_idx_to_poc_l0 = ppv_map_ref_idx_to_poc;
    ppv_map_ref_idx_to_poc_l1 = ppv_map_ref_idx_to_poc + POC_LIST_L0_TO_L1_DIFF;

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    for(edge = 0; edge < 4; edge++, ps_top_mv_pred = ps_cur_mv_pred - 4)
    {
        /*********************************************************************/
        /* Each iteration of this loop fills the four BS values of one HORIZ */
        /* edge and one BS value for each of the four VERT edges.            */
        /*********************************************************************/
        WORD32 i;
        UWORD32 u4_vert_idx = 24 - (edge << 3);
        UWORD32 u4_bs_horz = pu4_bs_table[edge];
        mv_pred_t *ps_left_mv_pred = ps_leftmost_mv_pred + (edge << 2);

        for(i = 0; i < 4; i++, ps_top_mv_pred++, ps_cur_mv_pred++)
        {
            WORD16 i2_cur_mv0, i2_cur_mv1, i16_curMv2, i16_curMv3;
            WORD8 i1_cur_ref0, i1_cur_ref1;
            void *pv_cur_pic_addr0, *pv_cur_pic_addr1;
            void *pv_nbr_pic_addr0, *pv_nbr_pic_addr1;

            /******************************************************/
            /* Each iteration of this inner loop computes a HORIZ */
            /* and a VERT BS value for a 4x4 block                */
            /******************************************************/
            UWORD32 u4_bs_vert = (pu4_bs_table[i + 4] >> u4_vert_idx) & 0xf;
            UWORD32 u4_horz_idx = 24 - (i << 3);

            /*****************************************************/
            /* check if vert Bs for this block is already set    */
            /*****************************************************/
            if(!u4_bs_vert)
            {
                WORD16 i2_left_mv0, i2_left_mv1, i2_left_mv2, i2_left_mv3;
                /************************************************************/
                /* If Bs is not set, use left edge and current edge mvs and */
                /* reference pictures addresses to evaluate Bs==1           */
                /************************************************************/
                i2_left_mv0 = ps_left_mv_pred->i2_mv[0];
                i2_left_mv1 = ps_left_mv_pred->i2_mv[1];
                i2_left_mv2 = ps_left_mv_pred->i2_mv[2];
                i2_left_mv3 = ps_left_mv_pred->i2_mv[3];

                i2_cur_mv0 = ps_cur_mv_pred->i2_mv[0];
                i2_cur_mv1 = ps_cur_mv_pred->i2_mv[1];
                i16_curMv2 = ps_cur_mv_pred->i2_mv[2];
                i16_curMv3 = ps_cur_mv_pred->i2_mv[3];
                i1_cur_ref0 = ps_cur_mv_pred->i1_ref_frame[0];
                i1_cur_ref1 = ps_cur_mv_pred->i1_ref_frame[1];
                pv_cur_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_cur_ref0];
                pv_cur_pic_addr1 = ppv_map_ref_idx_to_poc_l1[i1_cur_ref1];

                if(i)
                {
                    WORD8 i1_left_ref0, i1_left_ref1;
                    i1_left_ref0 = ps_left_mv_pred->i1_ref_frame[0];
                    i1_left_ref1 = ps_left_mv_pred->i1_ref_frame[1];
                    pv_nbr_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_left_ref0];
                    pv_nbr_pic_addr1 = ppv_map_ref_idx_to_poc_l1[i1_left_ref1];
                }
                else
                {
                    pv_nbr_pic_addr0 = ps_left_addr->u4_add[edge & 2];
                    pv_nbr_pic_addr1 = ps_left_addr->u4_add[1 + (edge & 2)];
                }

                {
                    UWORD32 u4_bs_temp1, u4_bs_temp2;
                    /*********************************************************/
                    /* If any motion vector component differs by more than 1 */
                    /* integer pel or if reference pictures are different Bs */
                    /* is set to 1. Note that this condition shall be met for*/
                    /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
                    /*********************************************************/

                    u4_bs_temp1 =
                                    ((ABS((i2_left_mv0 - i2_cur_mv0))
                                                    >= 4)
                                                    | (ABS((i2_left_mv1
                                                                    - i2_cur_mv1))
                                                                    >= i4_ver_mvlimit)
                                                    | (ABS((i2_left_mv2
                                                                    - i16_curMv2))
                                                                    >= 4)
                                                    | (ABS((i2_left_mv3
                                                                    - i16_curMv3))
                                                                    >= i4_ver_mvlimit));

                    u4_bs_temp2 =
                                    ((ABS((i2_left_mv0 - i16_curMv2))
                                                    >= 4)
                                                    | (ABS((i2_left_mv1
                                                                    - i16_curMv3))
                                                                    >= i4_ver_mvlimit)
                                                    | (ABS((i2_left_mv2
                                                                    - i2_cur_mv0))
                                                                    >= 4)
                                                    | (ABS((i2_left_mv3
                                                                    - i2_cur_mv1))
                                                                    >= i4_ver_mvlimit));

                    u4_bs_vert =
                                    ((pv_nbr_pic_addr0 != pv_cur_pic_addr0)
                                                    || (pv_nbr_pic_addr1
                                                                    != pv_cur_pic_addr1)
                                                    || u4_bs_temp1)
                                                    && ((pv_nbr_pic_addr0
                                                                    != pv_cur_pic_addr1)
                                                                    || (pv_nbr_pic_addr1
                                                                                    != pv_cur_pic_addr0)
                                                                    || u4_bs_temp2);

                    pu4_bs_table[i + 4] |= (u4_bs_vert << u4_vert_idx);
                }
            }

            /*****************************************************/
            /* check if horz Bs for this block is already set    */
            /*****************************************************/
            if(!((u4_bs_horz >> u4_horz_idx) & 0xf))
            {
                WORD16 i2_top_mv0, i2_top_mv1, i16_topMv2, i16_topMv3;
                /************************************************************/
                /* If Bs is not set, use top edge and current edge mvs and  */
                /* reference pictures addresses to evaluate Bs==1           */
                /************************************************************/
                i2_cur_mv0 = ps_cur_mv_pred->i2_mv[0];
                i2_cur_mv1 = ps_cur_mv_pred->i2_mv[1];
                i16_curMv2 = ps_cur_mv_pred->i2_mv[2];
                i16_curMv3 = ps_cur_mv_pred->i2_mv[3];
                i1_cur_ref0 = ps_cur_mv_pred->i1_ref_frame[0];
                i1_cur_ref1 = ps_cur_mv_pred->i1_ref_frame[1];

                i2_top_mv0 = ps_top_mv_pred->i2_mv[0];
                i2_top_mv1 = ps_top_mv_pred->i2_mv[1];
                i16_topMv2 = ps_top_mv_pred->i2_mv[2];
                i16_topMv3 = ps_top_mv_pred->i2_mv[3];
                pv_cur_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_cur_ref0];
                pv_cur_pic_addr1 = ppv_map_ref_idx_to_poc_l1[i1_cur_ref1];
                if(edge)
                {
                    WORD8 i1_top_ref0, i1_top_ref1;
                    i1_top_ref0 = ps_top_mv_pred->i1_ref_frame[0];
                    i1_top_ref1 = ps_top_mv_pred->i1_ref_frame[1];
                    pv_nbr_pic_addr0 = ppv_map_ref_idx_to_poc_l0[i1_top_ref0];
                    pv_nbr_pic_addr1 = ppv_map_ref_idx_to_poc_l1[i1_top_ref1];
                }
                else
                {
                    pv_nbr_pic_addr0 = u4_pic_addrress[i & 2];
                    pv_nbr_pic_addr1 = u4_pic_addrress[1 + (i & 2)];
                }

                {
                    UWORD32 u4_bs_temp1, u4_bs_temp2;
                    UWORD32 u4_bs;
                    /*********************************************************/
                    /* If any motion vector component differs by more than 1 */
                    /* integer pel or if reference pictures are different Bs */
                    /* is set to 1. Note that this condition shall be met for*/
                    /* both (fwd-fwd,bwd-bwd) and (fwd-bwd,bwd-fwd) direction*/
                    /*********************************************************/

                    u4_bs_temp1 =
                                    ((ABS((i2_top_mv0 - i2_cur_mv0))
                                                    >= 4)
                                                    | (ABS((i2_top_mv1
                                                                    - i2_cur_mv1))
                                                                    >= i4_ver_mvlimit)
                                                    | (ABS((i16_topMv2
                                                                    - i16_curMv2))
                                                                    >= 4)
                                                    | (ABS((i16_topMv3
                                                                    - i16_curMv3))
                                                                    >= i4_ver_mvlimit));

                    u4_bs_temp2 =
                                    ((ABS((i2_top_mv0 - i16_curMv2))
                                                    >= 4)
                                                    | (ABS((i2_top_mv1
                                                                    - i16_curMv3))
                                                                    >= i4_ver_mvlimit)
                                                    | (ABS((i16_topMv2
                                                                    - i2_cur_mv0))
                                                                    >= 4)
                                                    | (ABS((i16_topMv3
                                                                    - i2_cur_mv1))
                                                                    >= i4_ver_mvlimit));

                    u4_bs =
                                    ((pv_nbr_pic_addr0 != pv_cur_pic_addr0)
                                                    || (pv_nbr_pic_addr1
                                                                    != pv_cur_pic_addr1)
                                                    || u4_bs_temp1)
                                                    && ((pv_nbr_pic_addr0
                                                                    != pv_cur_pic_addr1)
                                                                    || (pv_nbr_pic_addr1
                                                                                    != pv_cur_pic_addr0)
                                                                    || u4_bs_temp2);

                    u4_bs_horz |= (u4_bs << u4_horz_idx);
                }
            }

            ps_left_mv_pred = ps_cur_mv_pred;
        }

        pu4_bs_table[edge] = u4_bs_horz;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs_xtra_left_edge_cur_fld                           */
/*                                                                           */
/*  Description   : This function fills boundray strength (= 2 or 1) for     */
/*                  xtra left mb edge when cur mb is field and left mb is    */
/*                  frame.                                                   */
/*  Inputs        :                                                          */
/*                                                                           */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    :                                                          */
/*                                                                           */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_fill_bs_xtra_left_edge_cur_fld(UWORD32 *pu4_bs, /* Base pointer of BS table */
                                           WORD32 u4_left_mb_t_csbp, /* left mbpair's top csbp   */
                                           WORD32 u4_left_mb_b_csbp, /* left mbpair's bottom csbp*/
                                           WORD32 u4_cur_mb_csbp, /* csbp of current mb */
                                           UWORD32 u4_cur_mb_top /* is top or bottom mb */

                                           )
{
    const UWORD32 *pu4_packed_bs = (const UWORD32 *)gau4_ih264d_packed_bs2;
    UWORD32 u4_cur, u4_left, u4_or;
    UNUSED(u4_cur_mb_top);

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    u4_left_mb_t_csbp = ((u4_left_mb_t_csbp & 0x0008) >> 3)
                    + ((u4_left_mb_t_csbp & 0x0080) >> 6)
                    + ((u4_left_mb_t_csbp & 0x0800) >> 9)
                    + ((u4_left_mb_t_csbp & 0x8000) >> 12);

    u4_left_mb_b_csbp = ((u4_left_mb_b_csbp & 0x0008) << 1)
                    + ((u4_left_mb_b_csbp & 0x0080) >> 2)
                    + ((u4_left_mb_b_csbp & 0x0800) >> 5)
                    + ((u4_left_mb_b_csbp & 0x8000) >> 8);

    /*********************************************************************/
    /* u4_cur = 0|0|0|0|0|0|0|0|12C|12C|8C|8C|4C|4C|0C|0C                */
    /*********************************************************************/
    u4_cur = (u4_cur_mb_csbp & 0x0001) + ((u4_cur_mb_csbp & 0x0001) << 1)
                    + ((u4_cur_mb_csbp & 0x0010) >> 2)
                    + ((u4_cur_mb_csbp & 0x0010) >> 1)
                    + ((u4_cur_mb_csbp & 0x0100) >> 4)
                    + ((u4_cur_mb_csbp & 0x0100) >> 3)
                    + ((u4_cur_mb_csbp & 0x1000) >> 6)
                    + ((u4_cur_mb_csbp & 0x1000) >> 5);

    /*********************************************************************/
    /* u4_left =0|0|0|0|0|0|0|0|15Lb|11Lb|7Lb|3Lb|15Lt|11Lt|7Lt|3Lt      */
    /*********************************************************************/
    u4_left = u4_left_mb_t_csbp + u4_left_mb_b_csbp;

    u4_or = (u4_cur | u4_left);
    /*********************************************************************/
    /* Fill vert edges (4,9) boundary strengths  using look up table     */
    /*********************************************************************/
    pu4_packed_bs += 16;
    pu4_bs[4] = pu4_packed_bs[u4_or & 0xF];
    pu4_bs[9] = pu4_packed_bs[(u4_or >> 4)];
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs_xtra_left_edge_cur_frm                           */
/*                                                                           */
/*  Description   : This function fills boundray strength (= 2 or 1) for     */
/*                  xtra left mb edge when cur mb is frame and left mb is    */
/*                  field.                                                   */
/*  Inputs        :                                                          */
/*                                                                           */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    :                                                          */
/*                                                                           */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_fill_bs_xtra_left_edge_cur_frm(UWORD32 *pu4_bs, /* Base pointer of BS table */
                                           WORD32 u4_left_mb_t_csbp, /* left mbpair's top csbp   */
                                           WORD32 u4_left_mb_b_csbp, /* left mbpair's bottom csbp*/
                                           WORD32 u4_cur_mb_csbp, /* csbp of current mb */
                                           UWORD32 u4_cur_mb_bot /* is top or bottom mb */

                                           )
{
    const UWORD32 *pu4_packed_bs = (const UWORD32 *)gau4_ih264d_packed_bs2;
    UWORD32 u4_cur, u4_left, u4_or;
    UWORD32 u4_right_shift = (u4_cur_mb_bot << 3);

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    u4_left_mb_t_csbp >>= u4_right_shift;
    u4_left_mb_b_csbp >>= u4_right_shift;

    u4_left_mb_t_csbp = ((u4_left_mb_t_csbp & 0x08) >> 3)
                    + ((u4_left_mb_t_csbp & 0x08) >> 2)
                    + ((u4_left_mb_t_csbp & 0x80) >> 5)
                    + ((u4_left_mb_t_csbp & 0x80) >> 4);

    u4_left_mb_b_csbp = ((u4_left_mb_b_csbp & 0x08) << 1)
                    + ((u4_left_mb_b_csbp & 0x08) << 2)
                    + ((u4_left_mb_b_csbp & 0x80) >> 1)
                    + ((u4_left_mb_b_csbp & 0x80));

    u4_cur = ((u4_cur_mb_csbp & 0x0001)) + ((u4_cur_mb_csbp & 0x0010) >> 3)
                    + ((u4_cur_mb_csbp & 0x0100) >> 6)
                    + ((u4_cur_mb_csbp & 0x1000) >> 9);

    u4_cur += (u4_cur << 4);

    u4_left = u4_left_mb_t_csbp + u4_left_mb_b_csbp;

    u4_or = (u4_cur | u4_left);
    /*********************************************************************/
    /* Fill vert edges (4,9) boundary strengths  using look up table     */
    /*********************************************************************/
    pu4_packed_bs += 16;
    pu4_bs[4] = pu4_packed_bs[u4_or & 0xF];
    pu4_bs[9] = pu4_packed_bs[(u4_or >> 4)];
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_fill_bs_xtra_top_edge                                    */
/*                                                                           */
/*  Description   : This function fills boundray strength (= 2 or 1) for     */
/*                  xtra top mb edge when cur mb is top mb of frame mb pair  */
/*                  and top mbpair is field coded.                           */
/*  Inputs        :                                                          */
/*                                                                           */
/*  Globals       : <Does it use any global variables?>                      */
/*  Processing    :                                                          */
/*                                                                           */
/*                                                                           */
/*  Outputs       : <What does the function produce?>                        */
/*  Returns       : <What does the function return?>                         */
/*                                                                           */
/*  Issues        : <List any issues or problems with this function>         */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         16 10 2008   Jay             Draft                                */
/*                                                                           */
/*****************************************************************************/
void ih264d_fill_bs_xtra_top_edge(UWORD32 *pu4_bs, /* Base pointer of BS table */
                                  WORD32 u4_topmb_t_csbp, /* top mbpair's top csbp   */
                                  WORD32 u4_topmb_b_csbp, /* top mbpair's bottom csbp*/
                                  WORD32 u4_cur_mb_csbp /* csbp of current mb */

                                  )
{
    const UWORD32 *pu4_packed_bs = (const UWORD32 *)gau4_ih264d_packed_bs2;
    UWORD32 u4_or;

    u4_cur_mb_csbp &= 0xf;
    u4_topmb_t_csbp >>= 12;
    u4_topmb_b_csbp >>= 12;

    u4_or = (u4_cur_mb_csbp | u4_topmb_t_csbp);
    /*********************************************************************/
    /* Fill vert edges (0,8) boundary strengths  using look up table     */
    /*********************************************************************/
    pu4_packed_bs += 16;
    pu4_bs[8] = pu4_packed_bs[u4_or];

    u4_or = (u4_cur_mb_csbp | u4_topmb_b_csbp);
    pu4_bs[0] = pu4_packed_bs[u4_or];
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_compute_bs_non_mbaff                                        */
/*                                                                           */
/*  Description   : This function computes the pointers of left,top & current*/
/*                : Nnz, MvPred & deblk_mb_t and supplies to FillBs function for*/
/*                : Boundary Strength Calculation                            */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Processing    : This functions calls deblock MB in the MB increment order*/
/*                                                                           */
/*  Outputs       : Produces the Boundary Strength for Current Mb            */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                      ITTIAM                                               */
/*****************************************************************************/

void ih264d_compute_bs_non_mbaff(dec_struct_t * ps_dec,
                                 dec_mb_info_t * ps_cur_mb_info,
                                 const UWORD16 u2_mbxn_mb)
{
    /* Mvpred and Nnz for top and Courrent */
    mv_pred_t *ps_cur_mv_pred, *ps_top_mv_pred = NULL, *ps_left_mv_pred;
    /* deblk_mb_t Params */
    deblk_mb_t *ps_cur_mb_params; /*< Parameters of current MacroBlock */
    deblkmb_neighbour_t *ps_deblk_top_mb;

    /* Reference Index to POC mapping*/
    void ** apv_map_ref_idx_to_poc;
    UWORD32 u4_leftmbtype;

    UWORD16 u2_left_csbp, u2_top_csbp, u2_cur_csbp;

    /* Set of flags */
    UWORD32 u4_cur_mb_intra, u1_top_mb_typ, u4_cur_mb_fld;
    UWORD32 u1_cur_mb_type;
    UWORD32 * pu4_bs_table;

    /* Neighbour availability */
    /* Initialization */
    const UWORD32 u2_mbx = ps_cur_mb_info->u2_mbx;
    const UWORD32 u2_mby = ps_cur_mb_info->u2_mby;
    const UWORD32 u1_pingpong = u2_mbx & 0x01;

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    ps_deblk_top_mb = ps_dec->ps_deblk_top_mb + u2_mbx;


    /* Pointer assignment for Current DeblkMB, Current Mv Pred  */
    ps_cur_mb_params = ps_dec->ps_deblk_mbn + u2_mbxn_mb;
    ps_cur_mv_pred = ps_dec->ps_mv_cur + (u2_mbxn_mb << 4);

    apv_map_ref_idx_to_poc = ps_dec->ppv_map_ref_idx_to_poc + 1;
    u1_cur_mb_type = ps_cur_mb_params->u1_mb_type;
    u1_top_mb_typ = ps_deblk_top_mb->u1_mb_type;
    ps_deblk_top_mb->u1_mb_type = u1_cur_mb_type;

    {
        UWORD8 mb_qp_temp;

        ps_cur_mb_params->u1_topmb_qp = ps_deblk_top_mb->u1_mb_qp;
        ps_deblk_top_mb->u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

        ps_cur_mb_params->u1_left_mb_qp = ps_dec->deblk_left_mb[1].u1_mb_qp;
        ps_dec->deblk_left_mb[1].u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

    }

    /* if no deblocking required for current Mb then continue */
    /* Check next Mbs   in Mb group                           */
    if(ps_cur_mb_params->u1_deblocking_mode & MB_DISABLE_FILTERING)
    {
        void ** pu4_map_ref_idx_to_poc_l1 = apv_map_ref_idx_to_poc +
        POC_LIST_L0_TO_L1_DIFF;
        {
            /* Store Parameter for Top MvPred refernce frame Address */

            void ** ppv_top_mv_pred_addr = ps_cur_mb_info->ps_curmb->u4_pic_addrress;
            WORD8 * p1_refTop0 = (ps_cur_mv_pred + 12)->i1_ref_frame;
            WORD8 * p1_refTop1 = (ps_cur_mv_pred + 14)->i1_ref_frame;

            /* Store Left addresses for Next Mb   */
            void ** ppv_left_mv_pred_addr =
                            ps_dec->ps_left_mvpred_addr[!u1_pingpong][1].u4_add;
            WORD8 * p1_refleft0 = (ps_cur_mv_pred + 3)->i1_ref_frame;


            ppv_top_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refTop0[0]];
            ppv_top_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refTop0[1]];

            ppv_left_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_top_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_left_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_top_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];

            ppv_left_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refleft0[0]];
            ppv_left_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refleft0[1]];
            //}
            /* Storing the leftMbtype for next Mb */
            ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;
        }

        return;
    }

    /* Flag for extra left Edge */
    ps_cur_mb_params->u1_single_call = 1;

    /* Update the Left deblk_mb_t and Left MvPred Parameters           */
    if(!u2_mbx)
    {
        u4_leftmbtype = 0;

        /* Initialize the ps_left_mv_pred with Junk but Valid Location */
        /* to avoid invalid memory access                           */
        /* this is read only pointer                                */
        ps_left_mv_pred = ps_dec->ps_mv_cur + 3;
    }
    else
    {
        u4_leftmbtype = ps_dec->deblk_left_mb[1].u1_mb_type;

        /* Come to Left Most Edge of the MB */
        ps_left_mv_pred = (u2_mbxn_mb) ?
                        ps_dec->ps_mv_cur + ((u2_mbxn_mb - 1) << 4) + 3 :
                        ps_dec->ps_mv_left + 3;
    }

    if(!u2_mby)
        u1_top_mb_typ = 0;

    /* MvPred Pointer Calculation */
    /* CHANGED CODE */
    ps_top_mv_pred = ps_cur_mv_pred - (ps_dec->u2_frm_wd_in_mbs << 4) + 12;

    u4_cur_mb_intra = u1_cur_mb_type & D_INTRA_MB;
    u4_cur_mb_fld = !!(u1_cur_mb_type & D_FLD_MB);
    /* Compute BS function */
    pu4_bs_table = ps_cur_mb_params->u4_bs_table;

    u2_cur_csbp = ps_cur_mb_info->ps_curmb->u2_luma_csbp;
    u2_left_csbp = ps_cur_mb_info->ps_left_mb->u2_luma_csbp;
    u2_top_csbp = ps_cur_mb_info->ps_top_mb->u2_luma_csbp;
    /* Compute BS function */
    if(ps_dec->ps_cur_sps->u1_profile_idc == HIGH_PROFILE_IDC)
    {
        if(ps_cur_mb_info->u1_tran_form8x8 == 1)
        {
            u2_cur_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_curmb->u2_luma_csbp);
        }

        if(ps_cur_mb_info->ps_left_mb->u1_tran_form8x8 == 1)
        {
            u2_left_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_left_mb->u2_luma_csbp);
        }

        if(ps_cur_mb_info->ps_top_mb->u1_tran_form8x8 == 1)
        {
            u2_top_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_top_mb->u2_luma_csbp);
        }
    }
    if(u4_cur_mb_intra)
    {

        pu4_bs_table[4] = 0x04040404;
        pu4_bs_table[0] = u4_cur_mb_fld ? 0x03030303 : 0x04040404;
        pu4_bs_table[1] = 0x03030303;
        pu4_bs_table[2] = 0x03030303;
        pu4_bs_table[3] = 0x03030303;
        pu4_bs_table[5] = 0x03030303;
        pu4_bs_table[6] = 0x03030303;
        pu4_bs_table[7] = 0x03030303;
    }
    else
    {
        UWORD32 u4_is_non16x16 = !!(u1_cur_mb_type & D_PRED_NON_16x16);
        UWORD32 u4_is_b = ps_dec->u1_B;

        ih264d_fill_bs2_horz_vert(
                        pu4_bs_table, u2_left_csbp, u2_top_csbp, u2_cur_csbp,
                        (const UWORD32 *)(gau4_ih264d_packed_bs2),
                        (const UWORD16 *)(gau2_ih264d_4x4_v2h_reorder));

        if(u4_leftmbtype & D_INTRA_MB)
            pu4_bs_table[4] = 0x04040404;

        if(u1_top_mb_typ & D_INTRA_MB)
            pu4_bs_table[0] = u4_cur_mb_fld ? 0x03030303 : 0x04040404;

        ps_dec->pf_fill_bs1[u4_is_b][u4_is_non16x16](
                        ps_cur_mv_pred, ps_top_mv_pred, apv_map_ref_idx_to_poc,
                        pu4_bs_table, ps_left_mv_pred,
                        &(ps_dec->ps_left_mvpred_addr[u1_pingpong][1]),
                        ps_cur_mb_info->ps_top_mb->u4_pic_addrress,
                        (4 >> u4_cur_mb_fld));
    }

    {
        void ** pu4_map_ref_idx_to_poc_l1 = apv_map_ref_idx_to_poc +
        POC_LIST_L0_TO_L1_DIFF;
        {
            /* Store Parameter for Top MvPred refernce frame Address */

            void ** ppv_top_mv_pred_addr = ps_cur_mb_info->ps_curmb->u4_pic_addrress;
            WORD8 * p1_refTop0 = (ps_cur_mv_pred + 12)->i1_ref_frame;
            WORD8 * p1_refTop1 = (ps_cur_mv_pred + 14)->i1_ref_frame;

            /* Store Left addresses for Next Mb   */
            void ** ppv_left_mv_pred_addr =
                            ps_dec->ps_left_mvpred_addr[!u1_pingpong][1].u4_add;
            WORD8 * p1_refleft0 = (ps_cur_mv_pred + 3)->i1_ref_frame;

            ppv_top_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refTop0[0]];
            ppv_top_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refTop0[1]];

            ppv_left_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_top_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_left_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_top_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];

            ppv_left_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refleft0[0]];
            ppv_left_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refleft0[1]];

            /* Storing the leftMbtype for next Mb */
            ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;

        }
    }

    /* For transform 8x8 disable deblocking of the intrernal edges of a 8x8 block */
    if(ps_cur_mb_info->u1_tran_form8x8)
    {
        pu4_bs_table[1] = 0;
        pu4_bs_table[3] = 0;
        pu4_bs_table[5] = 0;
        pu4_bs_table[7] = 0;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_compute_bs_mbaff                                           */
/*                                                                           */
/*  Description   : This function computes the pointers of left,top & current*/
/*                : Nnz, MvPred & deblk_mb_t and supplies to FillBs function for*/
/*                : Boundary Strength Calculation                            */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Processing    : This functions calls deblock MB in the MB increment order*/
/*                                                                           */
/*  Outputs       : Produces the Boundary Strength for Current Mb            */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*                      ITTIAM                                               */
/*****************************************************************************/

void ih264d_compute_bs_mbaff(dec_struct_t * ps_dec,
                             dec_mb_info_t * ps_cur_mb_info,
                             const UWORD16 u2_mbxn_mb)
{
    /* Mvpred and Nnz for top and Courrent */
    mv_pred_t *ps_cur_mv_pred, *ps_top_mv_pred = NULL, *ps_left_mv_pred;
    /* deblk_mb_t Params */
    deblk_mb_t *ps_cur_mb_params; /*< Parameters of current MacroBlock */
    neighbouradd_t * ps_left_ngbr;
    deblkmb_neighbour_t *ps_deblk_top_mb;
    /* Reference Index to POC mapping*/
    void ** apv_map_ref_idx_to_poc;

    UWORD32 u4_leftmbtype;


    UWORD16 u2_left_csbp, u2_top_csbp, u2_cur_csbp;

    /* Set of flags */
    UWORD32 u4_cur_mb_intra, u4_cur_mb_fld, u4_top_mb_fld, u1_top_mb_typ, u4_left_mb_fld;
    UWORD32 u1_cur_mb_type;
    UWORD32 * pu4_bs_table;
    const UWORD32 u4_bot_mb = (1 - ps_cur_mb_info->u1_topmb);
    /* Initialization */
    const UWORD32 u2_mbx = ps_cur_mb_info->u2_mbx;
    const UWORD32 u2_mby = ps_cur_mb_info->u2_mby;
    /* Load From u1_pingpong and Store in !u1_pingpong */
    const UWORD32 u1_pingpong = u2_mbx & 0x01;

    PROFILE_DISABLE_BOUNDARY_STRENGTH()

    ps_deblk_top_mb = ps_dec->ps_deblk_top_mb + (u2_mbx << 1);


    /************************************************/
    /* Initialize the left Mb type                  */
    /* Left MvPred                                  */
    /************************************************/

    if(!u2_mbx)
    {
        /************************************************************/
        /* Initialize the ps_left_mv_pred with Junk but Valid Location */
        /* to avoid invalid memory access                       */
        /* this is read only pointer                                */
        /************************************************************/
        ps_left_mv_pred = ps_dec->ps_mv_cur + 16;
    }
    else
    {
        /* Come to Left Most Edge of the MB */
        ps_left_mv_pred = (u2_mbxn_mb) ?
                        ps_dec->ps_mv_cur + ((u2_mbxn_mb - 1) << 5) + 3 :
                        ps_dec->ps_mv_left + 3;

        ps_left_mv_pred += (u4_bot_mb << 4);
    }

    u4_leftmbtype = ps_dec->deblk_left_mb[u4_bot_mb].u1_mb_type;

    ps_left_ngbr = &(ps_dec->ps_left_mvpred_addr[u1_pingpong][u4_bot_mb]);

    /************************************************/
    /* Pointer Assignment for Current Mb Parameters */
    /* Pointer Assignment for Current MvPred        */
    /************************************************/
    ps_cur_mb_params = ps_dec->ps_deblk_mbn + (u2_mbxn_mb << 1) + u4_bot_mb;
    u1_cur_mb_type = ps_cur_mb_params->u1_mb_type;

    ps_cur_mv_pred = ps_dec->ps_mv_cur + (u2_mbxn_mb << 5);
    ps_cur_mv_pred += (u4_bot_mb << 4);

    /********************************************/
    /* Pointer Assignment for Top Mb Parameters */
    /* Pointer Assignment for Top MvPred and    */
    /* Pointer Assignment for Top Nnz           */
    /********************************************/

    /* CHANGED CODE */
    ps_top_mv_pred = ps_cur_mv_pred - (ps_dec->u2_frm_wd_in_mbs << 5) + 12;

    u4_cur_mb_fld = !!(u1_cur_mb_type & D_FLD_MB);
    u4_left_mb_fld = !!(ps_dec->deblk_left_mb[0].u1_mb_type & D_FLD_MB);

    if(u4_left_mb_fld != u4_cur_mb_fld)
    {
        /* Flag for extra left Edge */
        ps_cur_mb_params->u1_single_call = 0;

        if(u4_bot_mb)
        {
            ps_left_ngbr--;
            ps_left_mv_pred -= 16;
        }
    }
    else
        ps_cur_mb_params->u1_single_call = 1;

    apv_map_ref_idx_to_poc = ps_dec->ppv_map_ref_idx_to_poc + 1;
    if(u4_cur_mb_fld)
    {
        if(u4_bot_mb)
        {
            apv_map_ref_idx_to_poc += BOT_LIST_FLD_L0;
        }
        else
        {
            apv_map_ref_idx_to_poc += TOP_LIST_FLD_L0;
        }
    }

    /**********************************************************/
    /* if no deblocking required for current Mb then continue */
    /**********************************************************/
    if(ps_cur_mb_params->u1_deblocking_mode & MB_DISABLE_FILTERING)
    {
        void ** pu4_map_ref_idx_to_poc_l1 = apv_map_ref_idx_to_poc +
        POC_LIST_L0_TO_L1_DIFF;

        {
            /* Store Parameter for Top MvPred refernce frame Address */

            void ** ppv_top_mv_pred_addr = ps_cur_mb_info->ps_curmb->u4_pic_addrress;
            void ** ppv_left_mv_pred_addr =
                            ps_dec->ps_left_mvpred_addr[!u1_pingpong][u4_bot_mb].u4_add;
            WORD8 * p1_refTop0 = (ps_cur_mv_pred + 12)->i1_ref_frame;
            WORD8 * p1_refTop1 = (ps_cur_mv_pred + 14)->i1_ref_frame;
            WORD8 * p1_refLeft0 = (ps_cur_mv_pred + 3)->i1_ref_frame;
            ppv_top_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refTop0[0]];
            ppv_top_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refTop0[1]];
            ppv_left_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_top_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_left_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_top_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_left_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refLeft0[0]];
            ppv_left_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refLeft0[1]];
        }
        if(u4_bot_mb)
        {
            /* store The Left Mb Type*/
            ps_dec->deblk_left_mb[0].u1_mb_type =
                            (ps_cur_mb_params - 1)->u1_mb_type;
            ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;

        }
        ps_deblk_top_mb[u4_bot_mb].u1_mb_type = u1_cur_mb_type;
        return;
    }

    if(u2_mby)
    {
        u1_top_mb_typ = ps_deblk_top_mb[1].u1_mb_type;
        u4_top_mb_fld = !!(u1_top_mb_typ & D_FLD_MB);

        if(!u4_bot_mb)
        {
            if(u4_top_mb_fld & u4_cur_mb_fld)
                u1_top_mb_typ = ps_deblk_top_mb[0].u1_mb_type;
            else
            {
                ps_top_mv_pred += 16;
            }
        }
    }
    else
    {
        u4_top_mb_fld = u4_cur_mb_fld;
        u1_top_mb_typ = 0;
    }

    if(u4_bot_mb & !u4_cur_mb_fld)
    {
        u1_top_mb_typ = ps_deblk_top_mb[0].u1_mb_type;
        u4_top_mb_fld = u4_cur_mb_fld;
        ps_top_mv_pred = ps_cur_mv_pred - 4;
    }

    pu4_bs_table = ps_cur_mb_params->u4_bs_table;
    u4_cur_mb_intra = u1_cur_mb_type & D_INTRA_MB;

    u2_cur_csbp = ps_cur_mb_info->ps_curmb->u2_luma_csbp;
    u2_left_csbp = ps_cur_mb_info->ps_left_mb->u2_luma_csbp;
    u2_top_csbp = ps_cur_mb_info->ps_top_mb->u2_luma_csbp;
    /* Compute BS function */
    if(ps_dec->ps_cur_sps->u1_profile_idc == HIGH_PROFILE_IDC)
    {

        if(ps_cur_mb_info->u1_tran_form8x8 == 1)
        {
            u2_cur_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_curmb->u2_luma_csbp);
        }

        if(ps_cur_mb_info->ps_left_mb->u1_tran_form8x8 == 1)
        {
            u2_left_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_left_mb->u2_luma_csbp);
        }

        if(ps_cur_mb_info->ps_top_mb->u1_tran_form8x8 == 1)
        {
            u2_top_csbp = ih264d_update_csbp_8x8(
                            ps_cur_mb_info->ps_top_mb->u2_luma_csbp);
        }
    }
    if(u4_cur_mb_intra)
    {

        pu4_bs_table[4] = 0x04040404;
        if((0 == u4_cur_mb_fld) && (0 == u4_top_mb_fld))
        {
            pu4_bs_table[0] = 0x04040404;
        }
        else
        {
            pu4_bs_table[0] = 0x03030303;
        }

        pu4_bs_table[1] = 0x03030303;
        pu4_bs_table[2] = 0x03030303;
        pu4_bs_table[3] = 0x03030303;
        pu4_bs_table[5] = 0x03030303;
        pu4_bs_table[6] = 0x03030303;
        pu4_bs_table[7] = 0x03030303;

        /*********************************************************************/
        /* Fill Bs of xtra top and left edge unconditionally to avoid checks */
        /*********************************************************************/
        pu4_bs_table[8] = 0x03030303;
        pu4_bs_table[9] = 0x04040404;
    }
    else
    {
        UWORD32 u4_is_non16x16 = !!(u1_cur_mb_type & D_PRED_NON_16x16);
        UWORD32 u4_is_b = ps_dec->u1_B;

        ih264d_fill_bs2_horz_vert(
                        pu4_bs_table, u2_left_csbp, u2_top_csbp, u2_cur_csbp,
                        (const UWORD32 *)(gau4_ih264d_packed_bs2),
                        (const UWORD16 *)(gau2_ih264d_4x4_v2h_reorder));

        if(u4_leftmbtype & D_INTRA_MB)
            pu4_bs_table[4] = 0x04040404;

        if(u1_top_mb_typ & D_INTRA_MB)
            pu4_bs_table[0] = u4_cur_mb_fld ? 0x03030303 : 0x04040404;
        else if(u4_cur_mb_fld != u4_top_mb_fld)
        {
            /****************************************************/
            /* Setting BS for mixed mode edge=1 when (Bs!=2)    */
            /****************************************************/
            pu4_bs_table[0] = (pu4_bs_table[0] >> 1) + 0x01010101;
        }

        {
            /* Call to Compute Boundary Strength for Extra Left Edge */
            if(u2_mbx
                            && !(ps_cur_mb_params->u1_deblocking_mode
                                            & MB_DISABLE_LEFT_EDGE))
            {
                if(u4_cur_mb_fld != u4_left_mb_fld)
                {
                    UWORD32 u4_left_mb_t_csbp =
                                    ps_cur_mb_info->ps_left_mb[0].u2_luma_csbp;
                    UWORD32 u4_left_mb_b_csbp =
                                    ps_cur_mb_info->ps_left_mb[1].u2_luma_csbp;
                    if(1 == ps_cur_mb_info->ps_left_mb[0].u1_tran_form8x8)
                    {
                        u4_left_mb_t_csbp = (UWORD32)ih264d_update_csbp_8x8(
                                        (UWORD16)u4_left_mb_t_csbp);
                    }

                    if(1 == ps_cur_mb_info->ps_left_mb[1].u1_tran_form8x8)
                    {
                        u4_left_mb_b_csbp = (UWORD32)ih264d_update_csbp_8x8(
                                        (UWORD16)u4_left_mb_b_csbp);
                    }
                    ps_dec->pf_fill_bs_xtra_left_edge[u4_cur_mb_fld](
                                    pu4_bs_table, u4_left_mb_t_csbp,
                                    u4_left_mb_b_csbp, u2_cur_csbp, u4_bot_mb);

                    if(ps_dec->deblk_left_mb[0].u1_mb_type & D_INTRA_MB)
                        pu4_bs_table[4] = 0x04040404;

                    if(ps_dec->deblk_left_mb[1].u1_mb_type & D_INTRA_MB)
                        pu4_bs_table[9] = 0x04040404;

                }
            }
            /* Call to Compute Boundary Strength for Extra Top Edge */
            if(u2_mby
                            && !(ps_cur_mb_params->u1_deblocking_mode
                                            & MB_DISABLE_TOP_EDGE))
            {
                if((((!u4_bot_mb) & (!u4_cur_mb_fld)) && u4_top_mb_fld))
                {
                    UWORD32 u4_topmb_t_csbp =
                                    ps_cur_mb_info->ps_top_mb[-1].u2_luma_csbp;
                    UWORD32 u4_topmb_b_csbp =
                                    ps_cur_mb_info->ps_top_mb[0].u2_luma_csbp;
                    if(1 == ps_cur_mb_info->ps_top_mb[-1].u1_tran_form8x8)
                    {
                        u4_topmb_t_csbp = (UWORD32)ih264d_update_csbp_8x8(
                                        (UWORD16)u4_topmb_t_csbp);
                    }

                    if(1 == ps_cur_mb_info->ps_top_mb[0].u1_tran_form8x8)
                    {
                        u4_topmb_b_csbp = (UWORD32)ih264d_update_csbp_8x8(
                                        (UWORD16)u4_topmb_b_csbp);
                    }
                    ih264d_fill_bs_xtra_top_edge(pu4_bs_table, u4_topmb_t_csbp,
                                                 u4_topmb_b_csbp, u2_cur_csbp);

                    if(ps_deblk_top_mb[0].u1_mb_type & D_INTRA_MB)
                        pu4_bs_table[8] = 0x03030303;

                    if(ps_deblk_top_mb[1].u1_mb_type & D_INTRA_MB)
                        pu4_bs_table[0] = 0x03030303;
                }
            }
        }

        ps_dec->pf_fill_bs1[u4_is_b][u4_is_non16x16](
                        ps_cur_mv_pred, ps_top_mv_pred, apv_map_ref_idx_to_poc,
                        pu4_bs_table, ps_left_mv_pred, ps_left_ngbr,
                        ps_cur_mb_info->ps_top_mb->u4_pic_addrress,
                        (4 >> u4_cur_mb_fld));
    }

    {
        void ** pu4_map_ref_idx_to_poc_l1 = apv_map_ref_idx_to_poc +
        POC_LIST_L0_TO_L1_DIFF;

        {
            /* Store Parameter for Top MvPred refernce frame Address */
            void ** ppv_top_mv_pred_addr = ps_cur_mb_info->ps_curmb->u4_pic_addrress;
            void ** ppv_left_mv_pred_addr =
                            ps_dec->ps_left_mvpred_addr[!u1_pingpong][u4_bot_mb].u4_add;
            WORD8 * p1_refTop0 = (ps_cur_mv_pred + 12)->i1_ref_frame;
            WORD8 * p1_refTop1 = (ps_cur_mv_pred + 14)->i1_ref_frame;
            WORD8 * p1_refLeft0 = (ps_cur_mv_pred + 3)->i1_ref_frame;
            ppv_top_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refTop0[0]];
            ppv_top_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refTop0[1]];
            ppv_left_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_top_mv_pred_addr[2] = apv_map_ref_idx_to_poc[p1_refTop1[0]];
            ppv_left_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_top_mv_pred_addr[3] = pu4_map_ref_idx_to_poc_l1[p1_refTop1[1]];
            ppv_left_mv_pred_addr[0] = apv_map_ref_idx_to_poc[p1_refLeft0[0]];
            ppv_left_mv_pred_addr[1] = pu4_map_ref_idx_to_poc_l1[p1_refLeft0[1]];
        }
        if(u4_bot_mb)
        {
            /* store The Left Mb Type*/
            ps_dec->deblk_left_mb[0].u1_mb_type =
                            (ps_cur_mb_params - 1)->u1_mb_type;
            ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;

        }
        ps_deblk_top_mb[u4_bot_mb].u1_mb_type = u1_cur_mb_type;
    }
    /* For transform 8x8 disable deblocking of the intrernal edges of a 8x8 block */
    if(ps_cur_mb_info->u1_tran_form8x8)
    {
        pu4_bs_table[1] = 0;
        pu4_bs_table[3] = 0;
        pu4_bs_table[5] = 0;
        pu4_bs_table[7] = 0;
    }

}



/*!
 **************************************************************************
 * \if Function name : ih264d_fill_bs_for_mb \endif
 *
 * \brief
 *    Determines the boundary strength (Bs), for the complete MB. Bs is
 *    determined for each block boundary between two neighbouring 4x4
 *    luma blocks, then packed in a UWORD32, first Bs placed in MSB and
 *    so on.  Such packed Bs values for all 8 edges are kept in an array.
 *
 * \return
 *    Returns the packed boundary strength(Bs)  MSB -> LSB Bs0|Bs1|Bs2|Bs3
 *
 **************************************************************************
 */

void ih264d_fill_bs_for_mb(deblk_mb_t * ps_cur_mb_params,
                           deblk_mb_t * ps_top_mb_params,
                           deblk_mb_t * ps_left_mb_params,
                           mv_pred_t *ps_cur_mv_pred,
                           mv_pred_t *ps_top_mv_pred,
                           UWORD8 *puc_cur_nnz,
                           UWORD8 *puc_top_nnz,
                           void **ppv_map_ref_idx_to_poc,
                           UWORD32 ui_mbAff,
                           UWORD32 ui_bs_table[], /* pointer to the BsTable array */
                           mv_pred_t *ps_leftmost_mv_pred,
                           neighbouradd_t *ps_left_addr,
                           neighbouradd_t *ps_top_add)
{
    UWORD32 u4_bs_horz = 0;
    UWORD8 edge, u1_top_intra = 0, u1_left_intra = 0;
    mv_pred_t *ps_left_mv_pred;
    WORD16 i2_cur_mv0, i2_cur_mv1, i16_curMv2, i16_curMv3;
    WORD16 i2_left_mv0, i2_left_mv1, i2_left_mv2, i2_left_mv3;
    WORD16 i2_top_mv0, i2_top_mv1, i16_topMv2, i16_topMv3;
    WORD8 i1_cur_ref0, i1_cur_ref1, i1_left_ref0, i1_left_ref1, i1_top_ref0, i1_top_ref1;
    UWORD8 uc_cur_nnz, uc_left_nnz, uc_top_nnz, u1_mb_type, uc_Bslice;
    void **ppv_map_ref_idx_to_poc_l0, **ppv_map_ref_idx_to_poc_l1;
    UWORD8 uc_temp;
    UWORD8 uc_cur_mb_fld, uc_top_mb_fld;
    UWORD32 c_mv_limit;

    u1_mb_type = ps_cur_mb_params->u1_mb_type;
    uc_Bslice = u1_mb_type & D_B_SLICE;
    ppv_map_ref_idx_to_poc_l0 = ppv_map_ref_idx_to_poc;
    ppv_map_ref_idx_to_poc_l1 = ppv_map_ref_idx_to_poc + POC_LIST_L0_TO_L1_DIFF;

    ps_top_mb_params = ps_top_mb_params ? ps_top_mb_params : ps_cur_mb_params;
    u1_top_intra = ps_top_mb_params->u1_mb_type & D_INTRA_MB;
    u1_left_intra = ps_left_mb_params->u1_mb_type & D_INTRA_MB;

    ui_bs_table[4] = 0x04040404; //Default for INTRA MB Boundary edges.
    uc_cur_mb_fld = (ps_cur_mb_params->u1_mb_type & D_FLD_MB) >> 7;
    uc_top_mb_fld = (ps_top_mb_params->u1_mb_type & D_FLD_MB) >> 7;

    c_mv_limit = 4 >> uc_cur_mb_fld;
    if((0 == uc_cur_mb_fld) && (0 == uc_top_mb_fld))
    {
        ui_bs_table[0] = 0x04040404;
    }
    else
    {
        ui_bs_table[0] = 0x03030303;
    }

    for(edge = 0; edge < 4;
                    edge++, ps_top_mv_pred = ps_cur_mv_pred - 4, puc_top_nnz =
                                    puc_cur_nnz - 4)
    {
        //Each iteration of this loop fills the four BS values of one HORIZ edge and
        //one BS value for each of the four VERT edges.
        WORD8 i = 0;
        UWORD8 uc_bs_horiz, uc_bs_vert;
        UWORD32 ui_cnd;
        void *ui_ref_pic_addr[4];
        UWORD8 uc_mixed_mode_edge;

        uc_mixed_mode_edge = 0;

        uc_temp = (ui_mbAff << 4) + 13;

        uc_cur_nnz = *(puc_cur_nnz - uc_temp);
        ps_left_mv_pred = ps_leftmost_mv_pred + (edge << 2);

        for(i = 0; i < 4; i++, ps_top_mv_pred++, ps_cur_mv_pred++)
        {
            //Each iteration of this inner loop computes a HORIZ
            //and a VERT BS value for a 4x4 block

            uc_left_nnz = uc_cur_nnz;
            uc_cur_nnz = *puc_cur_nnz++;
            uc_top_nnz = *puc_top_nnz++;

            //VERT edge is assigned BS values first
            ui_cnd = !(uc_left_nnz || uc_cur_nnz);
            uc_bs_vert = 2;

            if(ui_cnd)
            {
                i2_left_mv0 = ps_left_mv_pred->i2_mv[0];
                i2_left_mv1 = ps_left_mv_pred->i2_mv[1];
                i2_left_mv2 = ps_left_mv_pred->i2_mv[2];
                i2_left_mv3 = ps_left_mv_pred->i2_mv[3];

                i2_cur_mv0 = ps_cur_mv_pred->i2_mv[0];
                i2_cur_mv1 = ps_cur_mv_pred->i2_mv[1];
                i16_curMv2 = ps_cur_mv_pred->i2_mv[2];
                i16_curMv3 = ps_cur_mv_pred->i2_mv[3];
                i1_cur_ref0 = ps_cur_mv_pred->i1_ref_frame[0];
                i1_cur_ref1 = ps_cur_mv_pred->i1_ref_frame[1];
                ui_ref_pic_addr[2] = ppv_map_ref_idx_to_poc_l0[i1_cur_ref0];
                ui_ref_pic_addr[3] = ppv_map_ref_idx_to_poc_l1[i1_cur_ref1];

                if(i)
                {
                    i1_left_ref0 = ps_left_mv_pred->i1_ref_frame[0];
                    i1_left_ref1 = ps_left_mv_pred->i1_ref_frame[1];
                    ui_ref_pic_addr[0] = ppv_map_ref_idx_to_poc_l0[i1_left_ref0];
                    ui_ref_pic_addr[1] = ppv_map_ref_idx_to_poc_l1[i1_left_ref1];
                }
                else
                {
                    ui_ref_pic_addr[0] = ps_left_addr->u4_add[edge & 2];
                    ui_ref_pic_addr[1] = ps_left_addr->u4_add[1 + (edge & 2)];
                }
                if(!uc_Bslice)
                {
                    uc_bs_vert =
                                    (ui_ref_pic_addr[0] != ui_ref_pic_addr[2])
                                                    | (ABS((i2_left_mv0
                                                                    - i2_cur_mv0))
                                                                    >= 4)
                                                    | (ABS((i2_left_mv1
                                                                    - i2_cur_mv1))
                                                                    >= (UWORD8)c_mv_limit);
                }
                else
                {
                    UWORD8 uc_bs_temp1, uc_bs_temp2;

                    uc_bs_vert = 1;

                    uc_bs_temp1 =
                                    ((ABS((i2_left_mv0 - i2_cur_mv0))
                                                    >= 4)
                                                    | (ABS((i2_left_mv1
                                                                    - i2_cur_mv1))
                                                                    >= (UWORD8)c_mv_limit)
                                                    | (ABS((i2_left_mv2
                                                                    - i16_curMv2))
                                                                    >= 4)
                                                    | (ABS((i2_left_mv3
                                                                    - i16_curMv3))
                                                                    >= (UWORD8)c_mv_limit));

                    uc_bs_temp2 =
                                    ((ABS((i2_left_mv0 - i16_curMv2))
                                                    >= 4)
                                                    | (ABS((i2_left_mv1
                                                                    - i16_curMv3))
                                                                    >= (UWORD8)c_mv_limit)
                                                    | (ABS((i2_left_mv2
                                                                    - i2_cur_mv0))
                                                                    >= 4)
                                                    | (ABS((i2_left_mv3
                                                                    - i2_cur_mv1))
                                                                    >= (UWORD8)c_mv_limit));

                    uc_bs_vert =
                                    (((ui_ref_pic_addr[0] != ui_ref_pic_addr[2])
                                                    || (ui_ref_pic_addr[1]
                                                                    != ui_ref_pic_addr[3]))
                                                    || (uc_bs_temp1))
                                                    && (((ui_ref_pic_addr[0]
                                                                    != ui_ref_pic_addr[3])
                                                                    || (ui_ref_pic_addr[1]
                                                                                    != ui_ref_pic_addr[2]))
                                                                    || (uc_bs_temp2));

                }
            }
            //Fill the VERT BS, only if valid i.e.,
            //if it is a non-edge OR it is an edge, which is not yet filled
            uc_bs_vert = (!i && u1_left_intra) ? 4 : uc_bs_vert;
            ui_bs_table[i + 4] = (ui_bs_table[i + 4] << 8) | uc_bs_vert;

            //HORIZ edge is assigned BS values next
            ui_cnd = !(uc_top_nnz || uc_cur_nnz);
            uc_bs_horiz = 2;

            if(ui_cnd)
            {
                uc_mixed_mode_edge =
                                (0 == edge) ? (uc_top_mb_fld != uc_cur_mb_fld) : 0;
                ui_cnd = 1 - uc_mixed_mode_edge;
                uc_bs_horiz = uc_mixed_mode_edge;
            }

            if(ui_cnd)
            {
                i2_cur_mv0 = ps_cur_mv_pred->i2_mv[0];
                i2_cur_mv1 = ps_cur_mv_pred->i2_mv[1];
                i16_curMv2 = ps_cur_mv_pred->i2_mv[2];
                i16_curMv3 = ps_cur_mv_pred->i2_mv[3];
                i1_cur_ref0 = ps_cur_mv_pred->i1_ref_frame[0];
                i1_cur_ref1 = ps_cur_mv_pred->i1_ref_frame[1];

                i2_top_mv0 = ps_top_mv_pred->i2_mv[0];
                i2_top_mv1 = ps_top_mv_pred->i2_mv[1];
                i16_topMv2 = ps_top_mv_pred->i2_mv[2];
                i16_topMv3 = ps_top_mv_pred->i2_mv[3];
                ui_ref_pic_addr[2] = ppv_map_ref_idx_to_poc_l0[i1_cur_ref0];
                ui_ref_pic_addr[3] = ppv_map_ref_idx_to_poc_l1[i1_cur_ref1];
                if(edge)
                {
                    i1_top_ref0 = ps_top_mv_pred->i1_ref_frame[0];
                    i1_top_ref1 = ps_top_mv_pred->i1_ref_frame[1];
                    ui_ref_pic_addr[0] = ppv_map_ref_idx_to_poc_l0[i1_top_ref0];
                    ui_ref_pic_addr[1] = ppv_map_ref_idx_to_poc_l1[i1_top_ref1];
                }
                else
                {
                    ui_ref_pic_addr[0] = ps_top_add->u4_add[i & 2];
                    ui_ref_pic_addr[1] = ps_top_add->u4_add[1 + (i & 2)];
                }
                if(!uc_Bslice)
                {
                    uc_bs_horiz =
                                    (ui_ref_pic_addr[0] != ui_ref_pic_addr[2])
                                                    | (ABS((i2_top_mv0
                                                                    - i2_cur_mv0))
                                                                    >= 4)
                                                    | (ABS((i2_top_mv1
                                                                    - i2_cur_mv1))
                                                                    >= (UWORD8)c_mv_limit);
                }
                else
                {
                    UWORD8 uc_bs_temp1, uc_bs_temp2;

                    uc_bs_horiz = 1;

                    uc_bs_temp1 =
                                    ((ABS((i2_top_mv0 - i2_cur_mv0))
                                                    >= 4)
                                                    | (ABS((i2_top_mv1
                                                                    - i2_cur_mv1))
                                                                    >= (UWORD8)c_mv_limit)
                                                    | (ABS((i16_topMv2
                                                                    - i16_curMv2))
                                                                    >= 4)
                                                    | (ABS((i16_topMv3
                                                                    - i16_curMv3))
                                                                    >= (UWORD8)c_mv_limit));

                    uc_bs_temp2 =
                                    ((ABS((i2_top_mv0 - i16_curMv2))
                                                    >= 4)
                                                    | (ABS((i2_top_mv1
                                                                    - i16_curMv3))
                                                                    >= (UWORD8)c_mv_limit)
                                                    | (ABS((i16_topMv2
                                                                    - i2_cur_mv0))
                                                                    >= 4)
                                                    | (ABS((i16_topMv3
                                                                    - i2_cur_mv1))
                                                                    >= (UWORD8)c_mv_limit));

                    uc_bs_horiz =
                                    (((ui_ref_pic_addr[0] != ui_ref_pic_addr[2])
                                                    || (ui_ref_pic_addr[1]
                                                                    != ui_ref_pic_addr[3]))
                                                    || (uc_bs_temp1))
                                                    && (((ui_ref_pic_addr[0]
                                                                    != ui_ref_pic_addr[3])
                                                                    || (ui_ref_pic_addr[1]
                                                                                    != ui_ref_pic_addr[2]))
                                                                    || (uc_bs_temp2));

                }
            }
            ps_left_mv_pred = ps_cur_mv_pred;
            u4_bs_horz = (u4_bs_horz << 8) + uc_bs_horiz;
        }
        //Fill the HORIZ BS, only if valid i.e.,
        //if it is a non-edge OR it is an edge, which is not yet filled
        if(edge || (!edge && !u1_top_intra))
            ui_bs_table[edge] = u4_bs_horz;
    }
}

/*!
 **************************************************************************
 * \if Function name : ih264d_fill_bs_for_extra_left_edge \endif
 *
 * \brief
 *    Fills the boundary strength (Bs), for the top extra edge. ock
 *
 * \return
 *    Returns the packed boundary strength(Bs)  MSB -> LSB Bs0|Bs1|Bs2|Bs3
 *
 **************************************************************************
 */
void ih264d_fill_bs_for_extra_left_edge(deblk_mb_t *ps_cur_deblk_mb,
                                        deblk_mb_t *ps_leftDeblkMb,
                                        UWORD8* puc_cur_nnz,
                                        UWORD8 uc_botMb)
{
    /* Set the Flag in uc_deblocking_mode variable of current MB*/
    /* for mixed mode edge*/
    ps_cur_deblk_mb->u1_single_call = 0;

    if(ps_cur_deblk_mb->u1_mb_type & D_INTRA_MB)
    {
        ps_cur_deblk_mb->u4_bs_table[4] = 0x04040404;
        ps_cur_deblk_mb->u4_bs_table[9] = 0x04040404;
    }
    else if((ps_leftDeblkMb->u1_mb_type & D_INTRA_MB)
                    && ((ps_leftDeblkMb + 1)->u1_mb_type & D_INTRA_MB))
    {
        ps_cur_deblk_mb->u4_bs_table[4] = 0x04040404;
        ps_cur_deblk_mb->u4_bs_table[9] = 0x04040404;
    }
    else
    {
        /* Get strengths of left MB edge */
        UWORD32 u4_bs;
        UWORD8 uc_Bs;
        WORD32 i;
        UWORD32 ui_curMbFld;
        UWORD8 *puc_left_nnz;
        UWORD32 ui_bs_left_edge[2];

        ui_curMbFld = (ps_cur_deblk_mb->u1_mb_type & D_FLD_MB) >> 7;

        puc_left_nnz = puc_cur_nnz - 29;
        if((ui_curMbFld == 0) && uc_botMb)
        {
            puc_left_nnz -= 8;
        }
        else if(ui_curMbFld && uc_botMb)
        {
            puc_left_nnz -= 16;
        }

        if(ui_curMbFld)
        {
            if(ps_leftDeblkMb->u1_mb_type & D_INTRA_MB)
            {
                ui_bs_left_edge[0] = 0x04040404;
                puc_left_nnz += 16;
                puc_cur_nnz += 8;
            }
            else
            {
                u4_bs = 0;
                for(i = 4; i > 0; i--)
                {
                    uc_Bs = ((*puc_cur_nnz || *puc_left_nnz)) ? 2 : 1;
                    u4_bs = (u4_bs << 8) | uc_Bs;
                    puc_left_nnz += 4;
                    if(i & 0x01)
                        puc_cur_nnz += 4;
                }
                ui_bs_left_edge[0] = u4_bs;
            }

            if((ps_leftDeblkMb + 1)->u1_mb_type & D_INTRA_MB)
            {
                ui_bs_left_edge[1] = 0x04040404;
            }
            else
            {
                u4_bs = 0;
                for(i = 4; i > 0; i--)
                {
                    uc_Bs = ((*puc_cur_nnz || *puc_left_nnz)) ? 2 : 1;
                    u4_bs = (u4_bs << 8) | uc_Bs;
                    puc_left_nnz += 4;
                    if(i & 0x01)
                        puc_cur_nnz += 4;
                }
                ui_bs_left_edge[1] = u4_bs;
            }
        }
        else
        {
            UWORD8 *puc_curNnzB, *puc_leftNnzB;
            puc_curNnzB = puc_cur_nnz;
            puc_leftNnzB = puc_left_nnz + 16;
            if(ps_leftDeblkMb->u1_mb_type & D_INTRA_MB)
            {
                ui_bs_left_edge[0] = 0x04040404;
            }
            else
            {
                u4_bs = 0;
                for(i = 4; i > 0; i--, puc_cur_nnz += 4)
                {
                    uc_Bs = ((*puc_cur_nnz || *puc_left_nnz)) ? 2 : 1;
                    u4_bs = (u4_bs << 8) | uc_Bs;
                    if(i & 0x01)
                        puc_left_nnz += 4;
                }
                ui_bs_left_edge[0] = u4_bs;
            }

            if((ps_leftDeblkMb + 1)->u1_mb_type & D_INTRA_MB)
            {
                ui_bs_left_edge[1] = 0x04040404;
            }
            else
            {
                u4_bs = 0;
                for(i = 4; i > 0; i--, puc_curNnzB += 4)
                {
                    uc_Bs = ((*puc_curNnzB || *puc_leftNnzB)) ? 2 : 1;
                    u4_bs = (u4_bs << 8) | uc_Bs;
                    if(i & 0x01)
                        puc_leftNnzB += 4;
                }
                ui_bs_left_edge[1] = u4_bs;
            }
        }
        /* Copy The Values in Cur Deblk Mb Parameters */
        ps_cur_deblk_mb->u4_bs_table[4] = ui_bs_left_edge[0];
        ps_cur_deblk_mb->u4_bs_table[9] = ui_bs_left_edge[1];
    }

}

/*!
 **************************************************************************
 * \if Function name : ih264d_fill_bs_for_extra_top_edge \endif
 *
 * \brief
 *    Fills the boundary strength (Bs), for the top extra edge. ock
 *
 * \return
 *    Returns the packed boundary strength(Bs)  MSB -> LSB Bs0|Bs1|Bs2|Bs3
 *
 **************************************************************************
 */
void ih264d_fill_bs_for_extra_top_edge(deblk_mb_t *ps_cur_mb_params,
                                       UWORD8 u1_Edge0_mb_typ,
                                       UWORD8 u1_Edge1_mb_typ,
                                       UWORD8 *pu1_curNnz,
                                       UWORD8 *pu1_topNnz)
{
    UWORD32 u4_bs;
    UWORD8 uc_Bs;
    WORD32 i;
    UWORD8 *pu1_cur_nnz_tmp;
    UWORD8 *pu1_top_nnz_tmp;
    UWORD8 u1_top_edge;
    UWORD8 u1_top_mb_type;
    for(u1_top_edge = 0; u1_top_edge < 2; u1_top_edge++)
    {
        u1_top_mb_type = u1_top_edge ? u1_Edge1_mb_typ : u1_Edge0_mb_typ;
        pu1_cur_nnz_tmp = pu1_curNnz;
        pu1_top_nnz_tmp = pu1_topNnz + (u1_top_edge << 2);

        if((ps_cur_mb_params->u1_mb_type & D_INTRA_MB)
                        + (u1_top_mb_type & D_INTRA_MB))
        {
            u4_bs = 0x03030303;
        }
        else
        {
            u4_bs = 0;
            for(i = 4; i > 0; i--, pu1_cur_nnz_tmp += 1, pu1_top_nnz_tmp += 1)
            {
                uc_Bs = ((*pu1_cur_nnz_tmp || *pu1_top_nnz_tmp)) ? 2 : 1;
                u4_bs = (u4_bs << 8) | uc_Bs;
            }
        }
        if(u1_top_edge)
            ps_cur_mb_params->u4_bs_table[0] = u4_bs;
        else
            ps_cur_mb_params->u4_bs_table[8] = u4_bs;
    }
}


void ih264d_fill_bs_mbedge_4(dec_struct_t * ps_dec,
                             dec_mb_info_t * ps_cur_mb_info,
                             const UWORD16 u2_mbxn_mb)
{

    /* deblk_mb_t Params */
    deblk_mb_t *ps_cur_mb_params; /*< Parameters of current MacroBlock */
    deblkmb_neighbour_t *ps_deblk_top_mb;
    UWORD32 * pu4_bs_table;
    UWORD8 u1_cur_mb_type;

    /* Neighbour availability */
    /* Initialization */
    const UWORD32 u2_mbx = ps_cur_mb_info->u2_mbx;
    const UWORD32 u2_mby = ps_cur_mb_info->u2_mby;
    const UWORD32 u1_pingpong = u2_mbx & 0x01;
    ps_deblk_top_mb = ps_dec->ps_deblk_top_mb + u2_mbx;


    /* Pointer assignment for Current DeblkMB, Current Mv Pred  */
    ps_cur_mb_params = ps_dec->ps_deblk_mbn + u2_mbxn_mb;

    u1_cur_mb_type = ps_cur_mb_params->u1_mb_type;

    ps_deblk_top_mb->u1_mb_type = u1_cur_mb_type;

    {
        UWORD8 mb_qp_temp;

        ps_cur_mb_params->u1_topmb_qp = ps_deblk_top_mb->u1_mb_qp;
        ps_deblk_top_mb->u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

        ps_cur_mb_params->u1_left_mb_qp = ps_dec->deblk_left_mb[1].u1_mb_qp;
        ps_dec->deblk_left_mb[1].u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

    }

    ps_cur_mb_params->u1_single_call = 1;

    ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;
    /* if no deblocking required for current Mb then continue */
    /* Check next Mbs   in Mb group                           */
    if(ps_cur_mb_params->u1_deblocking_mode & MB_DISABLE_FILTERING)
    {
        /* Storing the leftMbtype for next Mb */
        return;
    }

    /* Compute BS function */
    pu4_bs_table = ps_cur_mb_params->u4_bs_table;

    pu4_bs_table[4] = 0x04040404;
    pu4_bs_table[0] = 0x04040404;
    pu4_bs_table[1] = 0;
    pu4_bs_table[2] = 0;
    pu4_bs_table[3] = 0;
    pu4_bs_table[5] = 0;
    pu4_bs_table[6] = 0;
    pu4_bs_table[7] = 0;

}

void ih264d_fill_bs_mbedge_2(dec_struct_t * ps_dec,
                             dec_mb_info_t * ps_cur_mb_info,
                             const UWORD16 u2_mbxn_mb)
{

    /* deblk_mb_t Params */
    deblk_mb_t *ps_cur_mb_params; /*< Parameters of current MacroBlock */
    deblkmb_neighbour_t *ps_deblk_top_mb;
    UWORD32 * pu4_bs_table;
    UWORD8 u1_cur_mb_type;

    /* Neighbour availability */
    /* Initialization */
    const UWORD32 u2_mbx = ps_cur_mb_info->u2_mbx;
    const UWORD32 u2_mby = ps_cur_mb_info->u2_mby;
    const UWORD32 u1_pingpong = u2_mbx & 0x01;
    ps_deblk_top_mb = ps_dec->ps_deblk_top_mb + u2_mbx;


    /* Pointer assignment for Current DeblkMB, Current Mv Pred  */
    ps_cur_mb_params = ps_dec->ps_deblk_mbn + u2_mbxn_mb;

    u1_cur_mb_type = ps_cur_mb_params->u1_mb_type;

    ps_deblk_top_mb->u1_mb_type = u1_cur_mb_type;

    {
        UWORD8 mb_qp_temp;

        ps_cur_mb_params->u1_topmb_qp = ps_deblk_top_mb->u1_mb_qp;
        ps_deblk_top_mb->u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

        ps_cur_mb_params->u1_left_mb_qp = ps_dec->deblk_left_mb[1].u1_mb_qp;
        ps_dec->deblk_left_mb[1].u1_mb_qp = ps_cur_mb_params->u1_mb_qp;

    }

    ps_cur_mb_params->u1_single_call = 1;

    ps_dec->deblk_left_mb[1].u1_mb_type = ps_cur_mb_params->u1_mb_type;
    /* if no deblocking required for current Mb then continue */
    /* Check next Mbs   in Mb group                           */
    if(ps_cur_mb_params->u1_deblocking_mode & MB_DISABLE_FILTERING)
    {
        /* Storing the leftMbtype for next Mb */
        return;
    }

    /* Compute BS function */
    pu4_bs_table = ps_cur_mb_params->u4_bs_table;

    {
        UWORD32 top_mb_csbp, left_mb_csbp, cur_mb_csbp;
        UWORD32 top_edge, left_edge;

        top_mb_csbp = ps_cur_mb_info->ps_top_mb->u2_luma_csbp;
        left_mb_csbp = ps_cur_mb_info->ps_left_mb->u2_luma_csbp;
        cur_mb_csbp = ps_cur_mb_info->ps_curmb->u2_luma_csbp;

        top_mb_csbp = top_mb_csbp >> 12;
        top_edge = top_mb_csbp | (cur_mb_csbp & 0xf);

        if(top_edge)
            pu4_bs_table[0] = 0x02020202;
        else
            pu4_bs_table[0] = 0;

        cur_mb_csbp = cur_mb_csbp & CSBP_LEFT_BLOCK_MASK;
        left_mb_csbp = left_mb_csbp & CSBP_RIGHT_BLOCK_MASK;

        left_edge = cur_mb_csbp | left_mb_csbp;

        if(left_edge)
            pu4_bs_table[4] = 0x02020202;
        else
            pu4_bs_table[4] = 0;

        pu4_bs_table[1] = 0;
        pu4_bs_table[2] = 0;
        pu4_bs_table[3] = 0;
        pu4_bs_table[5] = 0;
        pu4_bs_table[6] = 0;
        pu4_bs_table[7] = 0;
    }

}
