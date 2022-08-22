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

#include <string.h>

#include "ih264_typedefs.h"
#include "iv.h"
#include "ivd.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_debug.h"
#include "ih264d_defs.h"
#include "ih264d_defs.h"
#include "ih264d_structs.h"
#include "ih264d_deblocking.h"
#include "ih264d_mb_utils.h"
#include "ih264d_error_handler.h"
#include "ih264d_utils.h"


#include "ih264d_defs.h"
#include "ih264d_format_conv.h"
#include "ih264d_deblocking.h"
#include "ih264d_tables.h"

/*!
 *************************************************************************
 * \file ih264d_deblocking.c
 *
 * \brief
 *    Decoder specific deblocking routines
 *
 * \author AI
 *************************************************************************
 */

/*!
 **************************************************************************
 * \if Function name : HorizonPad \endif
 *
 * \brief
 *    Does the Horizontal padding on a whole pic.
 *
 * \return
 *    None
 **************************************************************************
 */

/*!
 **************************************************************************
 * \if Function name : FilterBoundaryLeft \endif
 *
 * \brief
 *    Filters MacroBlock Left Boundary egdes.
 *
 * \return
 *    None
 **************************************************************************
 */
void ih264d_filter_boundary_left_nonmbaff(dec_struct_t *ps_dec,
                                          tfr_ctxt_t * ps_tfr_cxt,
                                          WORD8 i1_cb_qp_idx_ofst,
                                          WORD8 i1_cr_qp_idx_ofst,
                                          deblk_mb_t * ps_cur_mb,
                                          WORD32 i4_strd_y,
                                          WORD32 i4_strd_uv,
                                          deblk_mb_t * ps_left_mb,
                                          UWORD32 pu4_bs_tab[],
                                          UWORD8 u1_cur_fld)
{
    UWORD8 *pu1_y, *pu1_u, *pu1_v;
    WORD32 uc_tmp, qp_avg;
    WORD32 alpha_u = 0, beta_u = 0, alpha_v = 0, beta_v = 0;
    WORD32 alpha_y = 0, beta_y = 0;

    WORD32 idx_b_u, idx_a_u, idx_b_v, idx_a_v;
    WORD32 idx_b_y, idx_a_y;

    UWORD32 u4_bs_val;

    UWORD8 *pu1_cliptab_u, *pu1_cliptab_v, *pu1_cliptab_y;

    UWORD8 u1_double_cl = !ps_cur_mb->u1_single_call;
    WORD32 ofst_a = ps_cur_mb->i1_slice_alpha_c0_offset;
    WORD32 ofst_b = ps_cur_mb->i1_slice_beta_offset;

    PROFILE_DISABLE_DEBLK()

    pu1_y = ps_tfr_cxt->pu1_mb_y;
    pu1_u = ps_tfr_cxt->pu1_mb_u;
    pu1_v = ps_tfr_cxt->pu1_mb_v;

    /* LUMA values */
    /* Deblock rounding change */
    qp_avg =
                    (UWORD8)((ps_cur_mb->u1_left_mb_qp + ps_cur_mb->u1_mb_qp + 1)
                                    >> 1);

    idx_a_y = qp_avg + ofst_a;
    alpha_y = gau1_ih264d_alpha_table[12 + idx_a_y];
    idx_b_y = qp_avg + ofst_b;
    beta_y = gau1_ih264d_beta_table[12 + idx_b_y];

    /* Chroma cb values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_cur_mb->u1_left_mb_qp + i1_cb_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }
    idx_a_u = qp_avg + ofst_a;
    alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
    idx_b_u = qp_avg + ofst_b;
    beta_u = gau1_ih264d_beta_table[12 + idx_b_u];
    /* Chroma cr values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_cur_mb->u1_left_mb_qp + i1_cr_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }
    idx_a_v = qp_avg + ofst_a;
    alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
    idx_b_v = qp_avg + ofst_b;
    beta_v = gau1_ih264d_beta_table[12 + idx_b_v];

    if(u1_double_cl == 0)
    {
        u4_bs_val = pu4_bs_tab[4];

        if(0x04040404 == u4_bs_val)
        {
            ps_dec->pf_deblk_luma_vert_bs4(pu1_y, i4_strd_y, alpha_y, beta_y);
            ps_dec->pf_deblk_chroma_vert_bs4(pu1_u, i4_strd_uv, alpha_u,
                                             beta_u, alpha_v, beta_v);
        }
        else
        {
            if(u4_bs_val)
            {

                pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y];
                pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u];
                pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v];
                ps_dec->pf_deblk_luma_vert_bslt4(pu1_y, i4_strd_y, alpha_y,
                                                 beta_y, u4_bs_val,
                                                 pu1_cliptab_y);
                ps_dec->pf_deblk_chroma_vert_bslt4(pu1_u, i4_strd_uv, alpha_u,
                                                   beta_u, alpha_v, beta_v,
                                                   u4_bs_val, pu1_cliptab_u,
                                                   pu1_cliptab_v);

            }
        }

    }
    else
    {

        i4_strd_y <<= (!u1_cur_fld);
        u4_bs_val = pu4_bs_tab[4];
        i4_strd_uv <<= (!u1_cur_fld);

        if(0x04040404 == u4_bs_val)
        {

            ps_dec->pf_deblk_luma_vert_bs4_mbaff(pu1_y, i4_strd_y, alpha_y,
                                                 beta_y);
            ps_dec->pf_deblk_chroma_vert_bs4_mbaff(pu1_u, i4_strd_uv, alpha_u,
                                                   beta_u, alpha_v, beta_v);

        }
        else
        {
            if(u4_bs_val)
            {

                pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y];
                pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u];
                pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v];

                ps_dec->pf_deblk_luma_vert_bslt4_mbaff(pu1_y, i4_strd_y,
                                                       alpha_y, beta_y,
                                                       u4_bs_val,
                                                       pu1_cliptab_y);
                ps_dec->pf_deblk_chroma_vert_bslt4_mbaff(pu1_u, i4_strd_uv,
                                                         alpha_u, beta_u,
                                                         alpha_v, beta_v,
                                                         u4_bs_val,
                                                         pu1_cliptab_u,
                                                         pu1_cliptab_v);
            }
        }

        {

            UWORD16 u2_shift = (i4_strd_y >> 1) << (u1_cur_fld ? 4 : 0);
            pu1_y += u2_shift;
            u2_shift = (i4_strd_uv >> 1) << (u1_cur_fld ? 3 : 0);
            pu1_u += u2_shift;
            pu1_v += u2_shift;
        }

        qp_avg = (((ps_left_mb + 1)->u1_mb_qp + ps_cur_mb->u1_mb_qp + 1) >> 1);

        idx_a_y = qp_avg + ofst_a;
        alpha_y = gau1_ih264d_alpha_table[12 + idx_a_y];
        idx_b_y = qp_avg + ofst_b;
        beta_y = gau1_ih264d_beta_table[12 + idx_b_y];
        u4_bs_val = pu4_bs_tab[9];

        {
            WORD32 mb_qp1, mb_qp2;
            mb_qp1 = ((ps_left_mb + 1)->u1_mb_qp + i1_cb_qp_idx_ofst);
            mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
            qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                            + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
        }
        idx_a_u = qp_avg + ofst_a;
        alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
        idx_b_u = qp_avg + ofst_b;
        beta_u = gau1_ih264d_beta_table[12 + idx_b_u];
        u4_bs_val = pu4_bs_tab[9];
        {
            WORD32 mb_qp1, mb_qp2;
            mb_qp1 = ((ps_left_mb + 1)->u1_mb_qp + i1_cr_qp_idx_ofst);
            mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
            qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                            + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
        }
        idx_a_v = qp_avg + ofst_a;
        alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
        idx_b_v = qp_avg + ofst_b;
        beta_v = gau1_ih264d_beta_table[12 + idx_b_v];

        if(0x04040404 == u4_bs_val)
        {
            ps_dec->pf_deblk_luma_vert_bs4_mbaff(pu1_y, i4_strd_y, alpha_y,
                                                 beta_y);
            ps_dec->pf_deblk_chroma_vert_bs4_mbaff(pu1_u, i4_strd_uv, alpha_u,
                                                   beta_u, alpha_v, beta_v);

        }
        else
        {
            if(u4_bs_val)
            {

                pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y];
                pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u];
                pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v];

                ps_dec->pf_deblk_luma_vert_bslt4_mbaff(pu1_y, i4_strd_y,
                                                       alpha_y, beta_y,
                                                       u4_bs_val,
                                                       pu1_cliptab_y);
                ps_dec->pf_deblk_chroma_vert_bslt4_mbaff(pu1_u, i4_strd_uv,
                                                         alpha_u, beta_u,
                                                         alpha_v, beta_v,
                                                         u4_bs_val,
                                                         pu1_cliptab_u,
                                                         pu1_cliptab_v);

            }
        }
    }

}

/*!
 **************************************************************************
 * \if Function name : FilterBoundaryTop \endif
 *
 * \brief
 *    Filters MacroBlock Top Boundary egdes.
 *
 * \return
 *    None
 **************************************************************************
 */

void ih264d_filter_boundary_top_nonmbaff(dec_struct_t *ps_dec,
                                         tfr_ctxt_t * ps_tfr_cxt,
                                         WORD8 i1_cb_qp_idx_ofst,
                                         WORD8 i1_cr_qp_idx_ofst,
                                         deblk_mb_t * ps_cur_mb,
                                         WORD32 i4_strd_y,
                                         WORD32 i4_strd_uv,
                                         deblk_mb_t * ps_top_mb,
                                         UWORD32 u4_bs)
{
    UWORD8 *pu1_y, *pu1_u;
    WORD32 alpha_u = 0, beta_u = 0, alpha_v = 0, beta_v = 0;
    WORD32 alpha_y = 0, beta_y = 0;
    WORD32 qp_avg;
    WORD32 idx_b_u, idx_a_u, idx_b_v, idx_a_v;
    WORD32 idx_b_y, idx_a_y;
    UWORD16 uc_tmp;

    UWORD8 *pu1_cliptab_u, *pu1_cliptab_v, *pu1_cliptab_y;
    WORD32 ofst_a = ps_cur_mb->i1_slice_alpha_c0_offset;
    WORD32 ofst_b = ps_cur_mb->i1_slice_beta_offset;

    UNUSED(ps_top_mb);
    /* LUMA values */
    /* Deblock rounding change */
    uc_tmp = ((ps_cur_mb->u1_topmb_qp + ps_cur_mb->u1_mb_qp + 1) >> 1);
    qp_avg = (UWORD8)uc_tmp;
    idx_a_y = qp_avg + ofst_a;
    alpha_y = gau1_ih264d_alpha_table[12 + idx_a_y];
    idx_b_y = qp_avg + ofst_b;
    beta_y = gau1_ih264d_beta_table[12 + idx_b_y];
    pu1_y = ps_tfr_cxt->pu1_mb_y;

    /* CHROMA cb values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_cur_mb->u1_topmb_qp + i1_cb_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }

    idx_a_u = qp_avg + ofst_a;
    alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
    idx_b_u = qp_avg + ofst_b;
    beta_u = gau1_ih264d_beta_table[12 + idx_b_u];
    /* CHROMA cr values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_cur_mb->u1_topmb_qp + i1_cr_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }

    idx_a_v = qp_avg + ofst_a;
    alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
    idx_b_v = qp_avg + ofst_b;
    beta_v = gau1_ih264d_beta_table[12 + idx_b_v];
    pu1_u = ps_tfr_cxt->pu1_mb_u;

    if(u4_bs == 0x04040404)
    {
        /* Code specific to the assembly module */

        ps_dec->pf_deblk_luma_horz_bs4(pu1_y, i4_strd_y, alpha_y, beta_y);
        ps_dec->pf_deblk_chroma_horz_bs4(pu1_u, i4_strd_uv, alpha_u, beta_u,
                                         alpha_v, beta_v);
    }
    else
    {
        if(u4_bs)
        {

            pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y];
            pu1_cliptab_u =
                            (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u];
            pu1_cliptab_v =
                            (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v];

            ps_dec->pf_deblk_luma_horz_bslt4(pu1_y, i4_strd_y, alpha_y, beta_y,
                                             u4_bs, pu1_cliptab_y);
            ps_dec->pf_deblk_chroma_horz_bslt4(pu1_u, i4_strd_uv, alpha_u,
                                               beta_u, alpha_v, beta_v,
                                               u4_bs, pu1_cliptab_u,
                                               pu1_cliptab_v);

        }
    }

}

void ih264d_deblock_mb_nonmbaff(dec_struct_t *ps_dec,
                                tfr_ctxt_t * ps_tfr_cxt,
                                WORD8 i1_cb_qp_idx_ofst,
                                WORD8 i1_cr_qp_idx_ofst,
                                WORD32 i4_strd_y,
                                WORD32 i4_strd_uv )
{
    UWORD8 *pu1_y, *pu1_u;
    UWORD32 u4_bs;

    WORD32 alpha, beta, alpha_u, beta_u, alpha_v, beta_v;

    UWORD8 *pu1_cliptab_u;
    UWORD8 *pu1_cliptab_v;
    UWORD8 *pu1_cliptab_y;

    UWORD32 * pu4_bs_tab;
    WORD32 idx_a_y, idx_a_u, idx_a_v;
    UWORD32 u4_deb_mode, u4_mbs_next;
    UWORD32 u4_image_wd_mb;
    deblk_mb_t *ps_top_mb,*ps_left_mb,*ps_cur_mb;

    PROFILE_DISABLE_DEBLK()
    /* Return from here to switch off deblocking */

    u4_image_wd_mb = ps_dec->u2_frm_wd_in_mbs;

    ps_cur_mb = ps_dec->ps_cur_deblk_mb;
    pu4_bs_tab = ps_cur_mb->u4_bs_table;
    u4_deb_mode = ps_cur_mb->u1_deblocking_mode;
     if(!(u4_deb_mode & MB_DISABLE_FILTERING))
     {

         if(ps_dec->u4_deblk_mb_x)
         {
             ps_left_mb = ps_cur_mb - 1;

         }
         else
         {
             ps_left_mb = NULL;

         }
         if(ps_dec->u4_deblk_mb_y != 0)
         {
             ps_top_mb = ps_cur_mb - (u4_image_wd_mb);
         }
         else
         {
             ps_top_mb = NULL;
         }

         if(u4_deb_mode & MB_DISABLE_LEFT_EDGE)
             ps_left_mb = NULL;
         if(u4_deb_mode & MB_DISABLE_TOP_EDGE)
             ps_top_mb = NULL;

        /*---------------------------------------------------------------------*/
        /* Filter wrt Left edge                                                */
        /* except                                                              */
        /*      - Left Egde is Picture Boundary                                */
        /*      - Left Egde is part of Slice Boundary and Deblocking           */
        /*        parameters of slice disable Filtering of Slice Boundary Edges*/
        /*---------------------------------------------------------------------*/
        if(ps_left_mb)
            ih264d_filter_boundary_left_nonmbaff(ps_dec, ps_tfr_cxt,
                                                 i1_cb_qp_idx_ofst,
                                                 i1_cr_qp_idx_ofst, ps_cur_mb,
                                                 i4_strd_y, i4_strd_uv, ps_left_mb,
                                                 pu4_bs_tab, 0);

        /*--------------------------------------------------------------------*/
        /* Filter wrt Other Vertical Edges                                    */
        /*--------------------------------------------------------------------*/
        {
            WORD32 ofst_a, ofst_b, idx_b_y, idx_b_u,
                            idx_b_v;
            WORD32 qp_avg, qp_avg_u, qp_avg_v;
            ofst_a = ps_cur_mb->i1_slice_alpha_c0_offset;
            ofst_b = ps_cur_mb->i1_slice_beta_offset;

            qp_avg = ps_cur_mb->u1_mb_qp;

            idx_a_y = qp_avg + ofst_a;
            alpha = gau1_ih264d_alpha_table[12 + idx_a_y];
            idx_b_y = qp_avg + ofst_b;
            beta = gau1_ih264d_beta_table[12 + idx_b_y];

            /* CHROMA values */
            /* CHROMA Cb values */
            qp_avg_u = (qp_avg + i1_cb_qp_idx_ofst);
            qp_avg_u = gau1_ih264d_qp_scale_cr[12 + qp_avg_u];
            idx_a_u = qp_avg_u + ofst_a;
            alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
            idx_b_u = qp_avg_u + ofst_b;
            beta_u = gau1_ih264d_beta_table[12 + idx_b_u];
            /* CHROMA Cr values */
            qp_avg_v = (qp_avg + i1_cr_qp_idx_ofst);
            qp_avg_v = gau1_ih264d_qp_scale_cr[12 + qp_avg_v];
            idx_a_v = qp_avg_v + ofst_a;
            alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
            idx_b_v = qp_avg_v + ofst_b;
            beta_v = gau1_ih264d_beta_table[12 + idx_b_v];
        }

        pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y]; //this for Luma
        pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u]; //this for chroma
        pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v]; //this for chroma

        //edge=1


        u4_bs = pu4_bs_tab[5];
        pu1_y = ps_tfr_cxt->pu1_mb_y;
        pu1_u = ps_tfr_cxt->pu1_mb_u;

        if(u4_bs)
        {

            ps_dec->pf_deblk_luma_vert_bslt4(pu1_y + 4, i4_strd_y, alpha, beta,
                                             u4_bs, pu1_cliptab_y);

        }
        //edge=2

        u4_bs = pu4_bs_tab[6];
        if(u4_bs)
        {
            ps_dec->pf_deblk_luma_vert_bslt4(pu1_y + 8, i4_strd_y, alpha, beta,
                                             u4_bs, pu1_cliptab_y);
            ps_dec->pf_deblk_chroma_vert_bslt4(pu1_u + 4 * YUV420SP_FACTOR,
                                               i4_strd_uv, alpha_u, beta_u,
                                               alpha_v, beta_v, u4_bs,
                                               pu1_cliptab_u, pu1_cliptab_v);

        }
        //edge=3

        u4_bs = pu4_bs_tab[7];
        if(u4_bs)
        {
            ps_dec->pf_deblk_luma_vert_bslt4(pu1_y + 12, i4_strd_y, alpha, beta,
                                             u4_bs, pu1_cliptab_y);

        }

        /*--------------------------------------------------------------------*/
        /* Filter wrt Top edge                                                */
        /* except                                                             */
        /*      - Top Egde is Picture Boundary                                */
        /*      - Top Egde is part of Slice Boundary and Deblocking           */
        /*        parameters of slice disable Filtering of Slice Boundary Edges*/
        /*--------------------------------------------------------------------*/
        if(ps_top_mb)
        {
            /** if top MB and MB AFF and cur MB is frame and top is field then  */
            /*  one extra top edge needs to be deblocked                        */

            ih264d_filter_boundary_top_nonmbaff(ps_dec, ps_tfr_cxt,
                                                i1_cb_qp_idx_ofst,
                                                i1_cr_qp_idx_ofst, ps_cur_mb,
                                                i4_strd_y, i4_strd_uv, ps_top_mb,
                                                pu4_bs_tab[0]);

        }

        /*--------------------------------------------------------------------*/
        /* Filter wrt Other Horizontal Edges                                  */
        /*--------------------------------------------------------------------*/

        //edge1
        u4_bs = pu4_bs_tab[1];

        if(u4_bs)
        {
            ps_dec->pf_deblk_luma_horz_bslt4(pu1_y + (i4_strd_y << 2), i4_strd_y,
                                             alpha, beta, u4_bs, pu1_cliptab_y);

        }
        //edge2
        u4_bs = pu4_bs_tab[2];

        if(u4_bs)
        {

            ps_dec->pf_deblk_luma_horz_bslt4(pu1_y + (i4_strd_y << 3), i4_strd_y,
                                             alpha, beta, u4_bs, pu1_cliptab_y);
            ps_dec->pf_deblk_chroma_horz_bslt4(pu1_u + (i4_strd_uv << 2),
                                               i4_strd_uv, alpha_u, beta_u,
                                               alpha_v, beta_v, u4_bs,
                                               pu1_cliptab_u, pu1_cliptab_v);

        }
        //edge3
        u4_bs = pu4_bs_tab[3];
        if(u4_bs)
        {
            ps_dec->pf_deblk_luma_horz_bslt4(
                            (pu1_y + (i4_strd_y << 3) + (i4_strd_y << 2)),
                            i4_strd_y, alpha, beta, u4_bs, pu1_cliptab_y);

        }
     }

     ps_dec->u4_deblk_mb_x++;
     ps_dec->ps_cur_deblk_mb++;
     ps_dec->u4_cur_deblk_mb_num++;
     u4_mbs_next = u4_image_wd_mb - ps_dec->u4_deblk_mb_x;

     ps_tfr_cxt->pu1_mb_y += 16;
     ps_tfr_cxt->pu1_mb_u += 8 * YUV420SP_FACTOR;
     ps_tfr_cxt->pu1_mb_v += 8;

     if(!u4_mbs_next)
     {
         ps_tfr_cxt->pu1_mb_y += ps_tfr_cxt->u4_y_inc;
         ps_tfr_cxt->pu1_mb_u += ps_tfr_cxt->u4_uv_inc;
         ps_tfr_cxt->pu1_mb_v += ps_tfr_cxt->u4_uv_inc;
         ps_dec->u4_deblk_mb_y++;
         ps_dec->u4_deblk_mb_x = 0;
     }

}

/**************************************************************************
 *
 *  Function Name : ih264d_init_deblk_tfr_ctxt
 *
 *  Description   : This function is called once per deblockpicture call
 *                  This sets up the transfer address contexts
 *
 *  Revision History:
 *
 *         DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *         14 06 2005   SWRN            Draft
 **************************************************************************/
void ih264d_init_deblk_tfr_ctxt(dec_struct_t * ps_dec,
                                pad_mgr_t *ps_pad_mgr,
                                tfr_ctxt_t *ps_tfr_cxt,
                                UWORD16 u2_image_wd_mb,
                                UWORD8 u1_mbaff)
{

    UWORD32 i4_wd_y;
    UWORD32 i4_wd_uv;
    UWORD8 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag; /*< Field u4_flag  */
    UNUSED(u2_image_wd_mb);
    ps_tfr_cxt->pu1_src_y = ps_dec->s_cur_pic.pu1_buf1 - 4;
    ps_tfr_cxt->pu1_src_u = ps_dec->s_cur_pic.pu1_buf2 - 4;
    ps_tfr_cxt->pu1_src_v = ps_dec->s_cur_pic.pu1_buf3 - 4;
    ps_tfr_cxt->pu1_dest_y = ps_tfr_cxt->pu1_src_y;
    ps_tfr_cxt->pu1_dest_u = ps_tfr_cxt->pu1_src_u;
    ps_tfr_cxt->pu1_dest_v = ps_tfr_cxt->pu1_src_v;

    ps_tfr_cxt->pu1_mb_y = ps_tfr_cxt->pu1_src_y + 4;
    ps_tfr_cxt->pu1_mb_u = ps_tfr_cxt->pu1_src_u + 4;
    ps_tfr_cxt->pu1_mb_v = ps_tfr_cxt->pu1_src_v + 4;

    i4_wd_y = ps_dec->u2_frm_wd_y << u1_field_pic_flag;
    i4_wd_uv = ps_dec->u2_frm_wd_uv << u1_field_pic_flag;
    ps_tfr_cxt->u4_y_inc = ((i4_wd_y << u1_mbaff) * 16
                    - (ps_dec->u2_frm_wd_in_mbs << 4));

    ps_tfr_cxt->u4_uv_inc = (i4_wd_uv << u1_mbaff) * 8
                    - (ps_dec->u2_frm_wd_in_mbs << 4);

    /* padding related initialisations */
    if(ps_dec->ps_cur_slice->u1_nal_ref_idc)
    {
        ps_pad_mgr->u1_vert_pad_top = !(ps_dec->ps_cur_slice->u1_field_pic_flag
                        && ps_dec->ps_cur_slice->u1_bottom_field_flag);
        ps_pad_mgr->u1_vert_pad_bot =
                        ((!ps_dec->ps_cur_slice->u1_field_pic_flag)
                                        || ps_dec->ps_cur_slice->u1_bottom_field_flag);
        ps_pad_mgr->u1_horz_pad = 1;
    }
    else
    {
        ps_pad_mgr->u1_horz_pad = 0;
        ps_pad_mgr->u1_vert_pad_top = 0;
        ps_pad_mgr->u1_vert_pad_bot = 0;
    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_deblock_picture_mbaff                                     */
/*                                                                           */
/*  Description   : This function carries out deblocking on a whole picture  */
/*                  with MBAFF                                               */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Processing    : This functions calls deblock MB in the MB increment order*/
/*                                                                           */
/*  Outputs       : Produces the deblocked picture                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         17 02 2005   NS              Creation                             */
/*         14 06 2005   SWRN            clean-up                             */
/*****************************************************************************/

void ih264d_deblock_picture_mbaff(dec_struct_t * ps_dec)
{
    WORD16 i2_mb_x, i2_mb_y;
    deblk_mb_t *ps_cur_mb;
    deblk_mb_t *ps_top_mb;
    deblk_mb_t *ps_left_mb;

    UWORD8 u1_vert_pad_top = 1;
    UWORD8 u1_cur_fld, u1_top_fld, u1_left_fld;
    UWORD8 u1_first_row;

    UWORD8 * pu1_deb_y, *pu1_deb_u, *pu1_deb_v;
    UWORD8 u1_deb_mode, u1_extra_top_edge;
    WORD32 i4_wd_y, i4_wd_uv;

    UWORD8 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag; /*< Field u4_flag                       */
    UWORD8 u1_bottom_field_flag = ps_dec->ps_cur_slice->u1_bottom_field_flag; /*< Bottom field u4_flag*/

    /**************************************************/
    /* one time loads from ps_dec which will be used  */
    /* frequently throughout the deblocking procedure */
    /**************************************************/
    pad_mgr_t * ps_pad_mgr = &ps_dec->s_pad_mgr;
    tfr_ctxt_t s_tfr_ctxt;
    tfr_ctxt_t * ps_tfr_cxt = &s_tfr_ctxt;

    UWORD16 u2_image_wd_mb = ps_dec->u2_frm_wd_in_mbs;
    UWORD16 u2_image_ht_mb = ps_dec->u2_frm_ht_in_mbs;
    UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;
    WORD8 i1_cb_qp_idx_ofst = ps_dec->ps_cur_pps->i1_chroma_qp_index_offset;
    WORD8 i1_cr_qp_idx_ofst =
                    ps_dec->ps_cur_pps->i1_second_chroma_qp_index_offset;

    /* Set up Parameter for  DMA transfer */
    ih264d_init_deblk_tfr_ctxt(ps_dec, ps_pad_mgr, ps_tfr_cxt, u2_image_wd_mb,
                               u1_mbaff);

    /* Pic level Initialisations */
    i2_mb_y = u2_image_ht_mb;
    i2_mb_x = 0;
    u1_extra_top_edge = 0;

    u1_first_row = 1;

    i4_wd_y = ps_dec->u2_frm_wd_y << u1_field_pic_flag;
    i4_wd_uv = ps_dec->u2_frm_wd_uv << u1_field_pic_flag;
    /* Initial filling of the buffers with deblocking data */

    pu1_deb_y = ps_tfr_cxt->pu1_mb_y;
    pu1_deb_u = ps_tfr_cxt->pu1_mb_u;
    pu1_deb_v = ps_tfr_cxt->pu1_mb_v;
    ps_cur_mb = ps_dec->ps_deblk_pic;

    if(ps_dec->u4_app_disable_deblk_frm == 0)
    {
        {

            while(i2_mb_y > 0)
            {
                do
                {

                    u1_deb_mode = ps_cur_mb->u1_deblocking_mode;
                    if(!(u1_deb_mode & MB_DISABLE_FILTERING))
                    {
                        ps_tfr_cxt->pu1_mb_y = pu1_deb_y;
                        ps_tfr_cxt->pu1_mb_u = pu1_deb_u;
                        ps_tfr_cxt->pu1_mb_v = pu1_deb_v;

                        u1_cur_fld = (ps_cur_mb->u1_mb_type & D_FLD_MB) >> 7;
                        u1_cur_fld &= 1;
                        if(i2_mb_x)
                        {
                            ps_left_mb = ps_cur_mb - 2;
                        }
                        else
                        {
                            ps_left_mb = NULL;
                        }
                        if(!u1_first_row)
                        {
                            ps_top_mb = ps_cur_mb - (u2_image_wd_mb << 1) + 1;
                            u1_top_fld = (ps_top_mb->u1_mb_type & D_FLD_MB)
                                            >> 7;
                        }
                        else
                        {
                            ps_top_mb = NULL;
                            u1_top_fld = 0;
                        }

                        if((!u1_first_row) & u1_top_fld & u1_cur_fld)
                            ps_top_mb--;

                        /********************************************************/
                        /* if top MB and MB AFF and cur MB is frame and top is  */
                        /* field, then one extra top edge needs to be deblocked */
                        /********************************************************/
                        u1_extra_top_edge = (!u1_cur_fld) & u1_top_fld;

                        if(u1_deb_mode & MB_DISABLE_LEFT_EDGE)
                            ps_left_mb = NULL;
                        if(u1_deb_mode & MB_DISABLE_TOP_EDGE)
                            ps_top_mb = NULL;

                        ih264d_deblock_mb_mbaff(ps_dec, ps_tfr_cxt,
                                                i1_cb_qp_idx_ofst,
                                                i1_cr_qp_idx_ofst, ps_cur_mb,
                                                i4_wd_y, i4_wd_uv, ps_top_mb,
                                                ps_left_mb, u1_cur_fld,
                                                u1_extra_top_edge);
                    }

                    ps_cur_mb++;

                    u1_deb_mode = ps_cur_mb->u1_deblocking_mode;
                    if(!(u1_deb_mode & MB_DISABLE_FILTERING))
                    {
                        ps_tfr_cxt->pu1_mb_y = pu1_deb_y;
                        ps_tfr_cxt->pu1_mb_u = pu1_deb_u;
                        ps_tfr_cxt->pu1_mb_v = pu1_deb_v;

                        u1_cur_fld = (ps_cur_mb->u1_mb_type & D_FLD_MB) >> 7;
                        u1_cur_fld &= 1;
                        if(i2_mb_x)
                        {
                            ps_left_mb = ps_cur_mb - 2;
                            u1_left_fld = (ps_left_mb->u1_mb_type & D_FLD_MB)
                                            >> 7;
                        }
                        else
                        {
                            ps_left_mb = NULL;
                            u1_left_fld = u1_cur_fld;
                        }
                        if(!u1_first_row)
                        {
                            ps_top_mb = ps_cur_mb - (u2_image_wd_mb << 1);
                        }
                        else
                        {
                            ps_top_mb = NULL;
                        }

                        {
                            UWORD8 u1_row_shift_y = 0, u1_row_shift_uv = 0;
                            if(!u1_cur_fld)
                            {
                                ps_top_mb = ps_cur_mb - 1;
                                u1_top_fld = (ps_top_mb->u1_mb_type & D_FLD_MB)
                                                >> 7;
                                u1_row_shift_y = 4;
                                u1_row_shift_uv = 3;
                            }
                            ps_tfr_cxt->pu1_mb_y += i4_wd_y << u1_row_shift_y;
                            ps_tfr_cxt->pu1_mb_u +=
                                            (i4_wd_uv << u1_row_shift_uv);
                            ps_tfr_cxt->pu1_mb_v += i4_wd_uv << u1_row_shift_uv;
                        }

                        /* point to A if top else A+1 */
                        if(u1_left_fld ^ u1_cur_fld)
                            ps_left_mb--;

                        /********************************************************/
                        /* if top MB and MB AFF and cur MB is frame and top is  */
                        /* field, then one extra top edge needs to be deblocked */
                        /********************************************************/
                        u1_extra_top_edge = 0;

                        if(u1_deb_mode & MB_DISABLE_LEFT_EDGE)
                            ps_left_mb = NULL;
                        if(u1_deb_mode & MB_DISABLE_TOP_EDGE)
                            ps_top_mb = NULL;

                        ih264d_deblock_mb_mbaff(ps_dec, ps_tfr_cxt,
                                                i1_cb_qp_idx_ofst,
                                                i1_cr_qp_idx_ofst, ps_cur_mb,
                                                i4_wd_y, i4_wd_uv, ps_top_mb,
                                                ps_left_mb, u1_cur_fld,
                                                u1_extra_top_edge);
                    }

                    ps_cur_mb++;
                    i2_mb_x++;

                    pu1_deb_y += 16;
                    pu1_deb_u += 8 * YUV420SP_FACTOR;
                    pu1_deb_v += 8;

                }
                while(u2_image_wd_mb > i2_mb_x);

                pu1_deb_y += ps_tfr_cxt->u4_y_inc;
                pu1_deb_u += ps_tfr_cxt->u4_uv_inc;
                pu1_deb_v += ps_tfr_cxt->u4_uv_inc;

                i2_mb_x = 0;
                i2_mb_y -= 2;

                u1_first_row = 0;

            }
        }

    }
    //Padd the Picture
    //Horizontal Padd

    if(ps_pad_mgr->u1_horz_pad)
    {
        UWORD32 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag;
        ps_dec->pf_pad_left_luma(ps_tfr_cxt->pu1_src_y + 4,
                                 ps_dec->u2_frm_wd_y << u1_field_pic_flag,
                                 ps_dec->u2_pic_ht >> u1_field_pic_flag,
                                 PAD_LEN_Y_H);
        ps_dec->pf_pad_right_luma(
                        ps_tfr_cxt->pu1_src_y + 4
                                        + (ps_dec->u2_frm_wd_in_mbs << 4),
                        ps_dec->u2_frm_wd_y << u1_field_pic_flag,
                        ps_dec->u2_pic_ht >> u1_field_pic_flag, PAD_LEN_Y_H);

        ps_dec->pf_pad_left_chroma(ps_tfr_cxt->pu1_src_u + 4,
                                   ps_dec->u2_frm_wd_uv << u1_field_pic_flag,
                                   (ps_dec->u2_pic_ht / 2) >> u1_field_pic_flag,
                                   PAD_LEN_UV_H * YUV420SP_FACTOR);
        ps_dec->pf_pad_right_chroma(
                        ps_tfr_cxt->pu1_src_u + 4
                                        + (ps_dec->u2_frm_wd_in_mbs << 4),
                        ps_dec->u2_frm_wd_uv << u1_field_pic_flag,
                        (ps_dec->u2_pic_ht / 2) >> u1_field_pic_flag,
                        PAD_LEN_UV_H * YUV420SP_FACTOR);

    }

//Vertical Padd Top
    if(ps_pad_mgr->u1_vert_pad_top)
    {
        ps_dec->pf_pad_top(ps_dec->ps_cur_pic->pu1_buf1 - PAD_LEN_Y_H,
                           ps_dec->u2_frm_wd_y, ps_dec->u2_frm_wd_y,
                           ps_pad_mgr->u1_pad_len_y_v);
        ps_dec->pf_pad_top(
                        ps_dec->ps_cur_pic->pu1_buf2
                                        - PAD_LEN_UV_H * YUV420SP_FACTOR,
                        ps_dec->u2_frm_wd_uv, ps_dec->u2_frm_wd_uv,
                        ps_pad_mgr->u1_pad_len_cr_v);
        ps_pad_mgr->u1_vert_pad_top = 0;
    }

//Vertical Padd Bottom
    if(ps_pad_mgr->u1_vert_pad_bot)
    {

        UWORD8 *pu1_buf;
        pu1_buf = ps_dec->ps_cur_pic->pu1_buf1 - PAD_LEN_Y_H;
        pu1_buf += ps_dec->u2_pic_ht * ps_dec->u2_frm_wd_y;
        ps_dec->pf_pad_bottom(pu1_buf, ps_dec->u2_frm_wd_y, ps_dec->u2_frm_wd_y,
                              ps_pad_mgr->u1_pad_len_y_v);
        pu1_buf = ps_dec->ps_cur_pic->pu1_buf2 - PAD_LEN_UV_H * YUV420SP_FACTOR;
        pu1_buf += (ps_dec->u2_pic_ht >> 1) * ps_dec->u2_frm_wd_uv;

        ps_dec->pf_pad_bottom(pu1_buf, ps_dec->u2_frm_wd_uv,
                              ps_dec->u2_frm_wd_uv,
                              ps_pad_mgr->u1_pad_len_cr_v);

    }
}

/*****************************************************************************/
/*                                                                           */
/*  Function Name : ih264d_deblock_picture_non_mbaff                                  */
/*                                                                           */
/*  Description   : This function carries out deblocking on a whole picture  */
/*                  without MBAFF                                            */
/*                                                                           */
/*  Inputs        : <What inputs does the function take?>                    */
/*  Processing    : This functions calls deblock MB in the MB increment order*/
/*                                                                           */
/*  Outputs       : Produces the deblocked picture                           */
/*  Returns       : None                                                     */
/*                                                                           */
/*  Revision History:                                                        */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         17 02 2005   NS              Creation                             */
/*         14 06 2005   SWRN            clean-up                             */
/*****************************************************************************/

void ih264d_deblock_picture_non_mbaff(dec_struct_t * ps_dec)
{
    deblk_mb_t *ps_cur_mb;

    UWORD8 u1_vert_pad_top = 1;

    UWORD8 u1_deb_mode;
    WORD32 i4_wd_y, i4_wd_uv;

    UWORD8 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag; /*< Field u4_flag                       */
    UWORD8 u1_bottom_field_flag = ps_dec->ps_cur_slice->u1_bottom_field_flag; /*< Bottom field u4_flag */

    /**************************************************/
    /* one time loads from ps_dec which will be used  */
    /* frequently throughout the deblocking procedure */
    /**************************************************/
    pad_mgr_t * ps_pad_mgr = &ps_dec->s_pad_mgr;
    tfr_ctxt_t s_tfr_ctxt;
    tfr_ctxt_t * ps_tfr_cxt = &s_tfr_ctxt; // = &ps_dec->s_tran_addrecon;

    UWORD16 u2_image_wd_mb = ps_dec->u2_frm_wd_in_mbs;
    UWORD16 u2_image_ht_mb = ps_dec->u2_frm_ht_in_mbs;
    WORD8 i1_cb_qp_idx_ofst = ps_dec->ps_cur_pps->i1_chroma_qp_index_offset;
    WORD8 i1_cr_qp_idx_ofst =
                    ps_dec->ps_cur_pps->i1_second_chroma_qp_index_offset;

    /* Set up Parameter for  DMA transfer */
    ih264d_init_deblk_tfr_ctxt(ps_dec, ps_pad_mgr, ps_tfr_cxt, u2_image_wd_mb,
                               0);

    /* Pic level Initialisations */



    i4_wd_y = ps_dec->u2_frm_wd_y << u1_field_pic_flag;
    i4_wd_uv = ps_dec->u2_frm_wd_uv << u1_field_pic_flag;
    /* Initial filling of the buffers with deblocking data */

    ps_cur_mb = ps_dec->ps_deblk_pic;

    if(ps_dec->u4_app_disable_deblk_frm == 0)
    {
        if(ps_dec->ps_cur_sps->u1_mb_aff_flag == 1)
        {
            while( ps_dec->u4_deblk_mb_y < u2_image_ht_mb)
            {
                ih264d_deblock_mb_nonmbaff(ps_dec, ps_tfr_cxt,
                                           i1_cb_qp_idx_ofst,
                                           i1_cr_qp_idx_ofst,
                                           i4_wd_y, i4_wd_uv);
                ps_cur_mb++;
            }
        }

    }

    //Padd the Picture
    //Horizontal Padd
    if(ps_pad_mgr->u1_horz_pad)
    {
        UWORD32 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag;
        ps_dec->pf_pad_left_luma(ps_tfr_cxt->pu1_src_y + 4,
                                 ps_dec->u2_frm_wd_y << u1_field_pic_flag,
                                 ps_dec->u2_pic_ht >> u1_field_pic_flag,
                                 PAD_LEN_Y_H);
        ps_dec->pf_pad_right_luma(
                        ps_tfr_cxt->pu1_src_y + 4
                                        + (ps_dec->u2_frm_wd_in_mbs << 4),
                        ps_dec->u2_frm_wd_y << u1_field_pic_flag,
                        ps_dec->u2_pic_ht >> u1_field_pic_flag, PAD_LEN_Y_H);

        ps_dec->pf_pad_left_chroma(ps_tfr_cxt->pu1_src_u + 4,
                                   ps_dec->u2_frm_wd_uv << u1_field_pic_flag,
                                   (ps_dec->u2_pic_ht / 2) >> u1_field_pic_flag,
                                   PAD_LEN_UV_H * YUV420SP_FACTOR);
        ps_dec->pf_pad_right_chroma(
                        ps_tfr_cxt->pu1_src_u + 4
                                        + (ps_dec->u2_frm_wd_in_mbs << 4),
                        ps_dec->u2_frm_wd_uv << u1_field_pic_flag,
                        (ps_dec->u2_pic_ht / 2) >> u1_field_pic_flag,
                        PAD_LEN_UV_H * YUV420SP_FACTOR);

    }

//Vertical Padd Top
    if(ps_pad_mgr->u1_vert_pad_top)
    {
        ps_dec->pf_pad_top(ps_dec->ps_cur_pic->pu1_buf1 - PAD_LEN_Y_H,
                           ps_dec->u2_frm_wd_y, ps_dec->u2_frm_wd_y,
                           ps_pad_mgr->u1_pad_len_y_v);
        ps_dec->pf_pad_top(
                        ps_dec->ps_cur_pic->pu1_buf2
                                        - PAD_LEN_UV_H * YUV420SP_FACTOR,
                        ps_dec->u2_frm_wd_uv, ps_dec->u2_frm_wd_uv,
                        ps_pad_mgr->u1_pad_len_cr_v);
        ps_pad_mgr->u1_vert_pad_top = 0;
    }

//Vertical Padd Bottom
    if(ps_pad_mgr->u1_vert_pad_bot)
    {

        UWORD8 *pu1_buf;
        pu1_buf = ps_dec->ps_cur_pic->pu1_buf1 - PAD_LEN_Y_H;
        pu1_buf += ps_dec->u2_pic_ht * ps_dec->u2_frm_wd_y;
        ps_dec->pf_pad_bottom(pu1_buf, ps_dec->u2_frm_wd_y, ps_dec->u2_frm_wd_y,
                              ps_pad_mgr->u1_pad_len_y_v);
        pu1_buf = ps_dec->ps_cur_pic->pu1_buf2 - PAD_LEN_UV_H * YUV420SP_FACTOR;
        pu1_buf += (ps_dec->u2_pic_ht >> 1) * ps_dec->u2_frm_wd_uv;

        ps_dec->pf_pad_bottom(pu1_buf, ps_dec->u2_frm_wd_uv,
                              ps_dec->u2_frm_wd_uv,
                              ps_pad_mgr->u1_pad_len_cr_v);

    }
}

void ih264d_deblock_picture_progressive(dec_struct_t * ps_dec)
{
    deblk_mb_t *ps_cur_mb;

    UWORD8 u1_vert_pad_top = 1;
    UWORD8 u1_mbs_next;
    UWORD8 u1_deb_mode;
    WORD32 i4_wd_y, i4_wd_uv;


    /**************************************************/
    /* one time loads from ps_dec which will be used  */
    /* frequently throughout the deblocking procedure */
    /**************************************************/
    pad_mgr_t * ps_pad_mgr = &ps_dec->s_pad_mgr;

    tfr_ctxt_t s_tfr_ctxt;
    tfr_ctxt_t * ps_tfr_cxt = &s_tfr_ctxt; // = &ps_dec->s_tran_addrecon;
    UWORD16 u2_image_wd_mb = ps_dec->u2_frm_wd_in_mbs;
    UWORD16 u2_image_ht_mb = ps_dec->u2_frm_ht_in_mbs;
    UWORD8 u1_mbaff = ps_dec->ps_cur_slice->u1_mbaff_frame_flag;

    WORD8 i1_cb_qp_idx_ofst = ps_dec->ps_cur_pps->i1_chroma_qp_index_offset;
    WORD8 i1_cr_qp_idx_ofst =
                    ps_dec->ps_cur_pps->i1_second_chroma_qp_index_offset;

    /* Set up Parameter for  deblocking */
    ih264d_init_deblk_tfr_ctxt(ps_dec, ps_pad_mgr, ps_tfr_cxt, u2_image_wd_mb,
                               0);

    /* Pic level Initialisations */

    i4_wd_y = ps_dec->u2_frm_wd_y;
    i4_wd_uv = ps_dec->u2_frm_wd_uv;
    /* Initial filling of the buffers with deblocking data */
    ps_cur_mb = ps_dec->ps_deblk_pic;

    if(ps_dec->u4_app_disable_deblk_frm == 0)
    {
        if(ps_dec->ps_cur_sps->u1_mb_aff_flag == 1)
        {
            while( ps_dec->u4_deblk_mb_y < u2_image_ht_mb)
            {
                ih264d_deblock_mb_nonmbaff(ps_dec, ps_tfr_cxt,
                                           i1_cb_qp_idx_ofst,
                                           i1_cr_qp_idx_ofst,
                                           i4_wd_y, i4_wd_uv);
                ps_cur_mb++;
            }
        }

    }

    //Padd the Picture
    //Horizontal Padd
    if(ps_pad_mgr->u1_horz_pad)
    {
        UWORD32 u1_field_pic_flag = ps_dec->ps_cur_slice->u1_field_pic_flag;
        ps_dec->pf_pad_left_luma(ps_tfr_cxt->pu1_src_y + 4,
                                 ps_dec->u2_frm_wd_y << u1_field_pic_flag,
                                 ps_dec->u2_pic_ht >> u1_field_pic_flag,
                                 PAD_LEN_Y_H);
        ps_dec->pf_pad_right_luma(
                        ps_tfr_cxt->pu1_src_y + 4
                                        + (ps_dec->u2_frm_wd_in_mbs << 4),
                        ps_dec->u2_frm_wd_y << u1_field_pic_flag,
                        ps_dec->u2_pic_ht >> u1_field_pic_flag, PAD_LEN_Y_H);

        ps_dec->pf_pad_left_chroma(ps_tfr_cxt->pu1_src_u + 4,
                                   ps_dec->u2_frm_wd_uv << u1_field_pic_flag,
                                   (ps_dec->u2_pic_ht / 2) >> u1_field_pic_flag,
                                   PAD_LEN_UV_H * YUV420SP_FACTOR);
        ps_dec->pf_pad_right_chroma(
                        ps_tfr_cxt->pu1_src_u + 4
                                        + (ps_dec->u2_frm_wd_in_mbs << 4),
                        ps_dec->u2_frm_wd_uv << u1_field_pic_flag,
                        (ps_dec->u2_pic_ht / 2) >> u1_field_pic_flag,
                        PAD_LEN_UV_H * YUV420SP_FACTOR);

    }

//Vertical Padd Top
    if(ps_pad_mgr->u1_vert_pad_top)
    {
        ps_dec->pf_pad_top(ps_dec->ps_cur_pic->pu1_buf1 - PAD_LEN_Y_H,
                           ps_dec->u2_frm_wd_y, ps_dec->u2_frm_wd_y,
                           ps_pad_mgr->u1_pad_len_y_v);
        ps_dec->pf_pad_top(
                        ps_dec->ps_cur_pic->pu1_buf2
                                        - PAD_LEN_UV_H * YUV420SP_FACTOR,
                        ps_dec->u2_frm_wd_uv, ps_dec->u2_frm_wd_uv,
                        ps_pad_mgr->u1_pad_len_cr_v);

    }

//Vertical Padd Bottom
    if(ps_pad_mgr->u1_vert_pad_bot)
    {

        UWORD8 *pu1_buf;
        pu1_buf = ps_dec->ps_cur_pic->pu1_buf1 - PAD_LEN_Y_H;
        pu1_buf += ps_dec->u2_pic_ht * ps_dec->u2_frm_wd_y;
        ps_dec->pf_pad_bottom(pu1_buf, ps_dec->u2_frm_wd_y, ps_dec->u2_frm_wd_y,
                              ps_pad_mgr->u1_pad_len_y_v);
        pu1_buf = ps_dec->ps_cur_pic->pu1_buf2 - PAD_LEN_UV_H * YUV420SP_FACTOR;
        pu1_buf += (ps_dec->u2_pic_ht >> 1) * ps_dec->u2_frm_wd_uv;

        ps_dec->pf_pad_bottom(pu1_buf, ps_dec->u2_frm_wd_uv,
                              ps_dec->u2_frm_wd_uv,
                              ps_pad_mgr->u1_pad_len_cr_v);

    }
}

/*!
 **************************************************************************
 * \if Function name : ih264d_set_deblocking_parameters \endif
 *
 * \brief
 *    Sets the deblocking parameters of the macroblock
 *
 * \return
 *    0 on Success and Error code otherwise
 *
 * \note
 *   Given the neighbour availablity information, and the deblocking
 *   parameters of the slice,this function will set the deblocking
 *   mode of the macroblock.
 **************************************************************************
 */

WORD8 ih264d_set_deblocking_parameters(deblk_mb_t * ps_cur_mb,
                                       dec_slice_params_t * ps_slice,
                                       UWORD8 u1_mb_ngbr_availablity,
                                       UWORD8 u1_mb_field_decoding_flag)
{
    /*------------------------------------------------------------------*/
    /* Set the deblocking parameters                                  */
    /*------------------------------------------------------------------*/
    ps_cur_mb->i1_slice_alpha_c0_offset = ps_slice->i1_slice_alpha_c0_offset;
    ps_cur_mb->i1_slice_beta_offset = ps_slice->i1_slice_beta_offset;
    ps_cur_mb->u1_mb_type = (u1_mb_field_decoding_flag << 7);

    switch(ps_slice->u1_disable_dblk_filter_idc)
    {
        case DBLK_ENABLED:
            ps_cur_mb->u1_deblocking_mode = MB_ENABLE_FILTERING;
            break;
        case DBLK_DISABLED:
            ps_cur_mb->u1_deblocking_mode = MB_DISABLE_FILTERING;
            break;
        case SLICE_BOUNDARY_DBLK_DISABLED:
        {
            ps_cur_mb->u1_deblocking_mode = MB_ENABLE_FILTERING;
            if(!(u1_mb_ngbr_availablity & LEFT_MB_AVAILABLE_MASK))
                ps_cur_mb->u1_deblocking_mode |= MB_DISABLE_LEFT_EDGE;
            if(!(u1_mb_ngbr_availablity & TOP_MB_AVAILABLE_MASK))
                ps_cur_mb->u1_deblocking_mode |= MB_DISABLE_TOP_EDGE;
            break;
        }
    }

    return (0);
}

void ih264d_copy_intra_pred_line(dec_struct_t *ps_dec,
                                 dec_mb_info_t *ps_cur_mb_info,
                                 UWORD32 nmb_index)
{
    UWORD8 *pu1_mb_last_row, u1_mb_field_decoding_flag;
    UWORD32 u4_recWidth, u4_recwidth_cr;

    u1_mb_field_decoding_flag = ps_cur_mb_info->u1_mb_field_decodingflag;

    u4_recWidth = ps_dec->u2_frm_wd_y << u1_mb_field_decoding_flag;
    u4_recwidth_cr = ps_dec->u2_frm_wd_uv << u1_mb_field_decoding_flag;

    pu1_mb_last_row = ps_dec->ps_frame_buf_ip_recon->pu1_dest_y
                    + (u4_recWidth * (MB_SIZE - 1));
    pu1_mb_last_row += MB_SIZE * nmb_index;
    MEMCPY_16BYTES(ps_dec->pu1_cur_y_intra_pred_line, pu1_mb_last_row);

    pu1_mb_last_row = ps_dec->ps_frame_buf_ip_recon->pu1_dest_u
                    + (u4_recwidth_cr * (BLK8x8SIZE - 1));
    pu1_mb_last_row += BLK8x8SIZE * nmb_index * YUV420SP_FACTOR;

    MEMCPY_16BYTES(ps_dec->pu1_cur_u_intra_pred_line, pu1_mb_last_row);

    ps_dec->pu1_cur_y_intra_pred_line = ps_dec->pu1_cur_y_intra_pred_line_base
                    + (MB_SIZE * (ps_cur_mb_info->u2_mbx + 1));
    ps_dec->pu1_cur_u_intra_pred_line = ps_dec->pu1_cur_u_intra_pred_line_base
                    + (BLK8x8SIZE * (ps_cur_mb_info->u2_mbx + 1))
                                    * YUV420SP_FACTOR;
    ps_dec->pu1_cur_v_intra_pred_line = ps_dec->pu1_cur_v_intra_pred_line_base
                    + (BLK8x8SIZE * (ps_cur_mb_info->u2_mbx + 1));

    if(ps_cur_mb_info->u2_mbx == (ps_dec->u2_frm_wd_in_mbs - 1))
    {
        UWORD8* pu1_temp;

        ps_dec->pu1_cur_y_intra_pred_line =
                        ps_dec->pu1_cur_y_intra_pred_line_base;
        ps_dec->pu1_cur_u_intra_pred_line =
                        ps_dec->pu1_cur_u_intra_pred_line_base;
        ps_dec->pu1_cur_v_intra_pred_line =
                        ps_dec->pu1_cur_v_intra_pred_line_base;

        /*swap current and previous rows*/
        pu1_temp = ps_dec->pu1_cur_y_intra_pred_line;
        ps_dec->pu1_cur_y_intra_pred_line = ps_dec->pu1_prev_y_intra_pred_line;
        ps_dec->pu1_prev_y_intra_pred_line = pu1_temp;

        pu1_temp = ps_dec->pu1_cur_u_intra_pred_line;
        ps_dec->pu1_cur_u_intra_pred_line = ps_dec->pu1_prev_u_intra_pred_line;
        ps_dec->pu1_prev_u_intra_pred_line = pu1_temp;

        pu1_temp = ps_dec->pu1_cur_v_intra_pred_line;
        ps_dec->pu1_cur_v_intra_pred_line = ps_dec->pu1_prev_v_intra_pred_line;
        ps_dec->pu1_prev_v_intra_pred_line = pu1_temp;

        ps_dec->pu1_cur_y_intra_pred_line_base =
                        ps_dec->pu1_cur_y_intra_pred_line;
        ps_dec->pu1_cur_u_intra_pred_line_base =
                        ps_dec->pu1_cur_u_intra_pred_line;
        ps_dec->pu1_cur_v_intra_pred_line_base =
                        ps_dec->pu1_cur_v_intra_pred_line;





    }

}


void ih264d_filter_boundary_left_mbaff(dec_struct_t *ps_dec,
                                       tfr_ctxt_t * ps_tfr_cxt,
                                       WORD8 i1_cb_qp_idx_ofst,
                                       WORD8 i1_cr_qp_idx_ofst,
                                       deblk_mb_t * ps_cur_mb,
                                       WORD32 i4_strd_y,
                                       WORD32 i4_strd_uv,
                                       deblk_mb_t * ps_left_mb, /* Neighbouring MB parameters   */
                                       UWORD32 pu4_bs_tab[], /* pointer to the BsTable array */
                                       UWORD8 u1_cur_fld)
{
    UWORD8 *pu1_y, *pu1_u, *pu1_v;
    UWORD8 uc_tmp, qp_avg;
    WORD32 alpha_u = 0, beta_u = 0, alpha_v = 0, beta_v = 0;
    WORD32 alpha_y = 0, beta_y = 0;

    WORD32 idx_b_u, idx_a_u, idx_b_v, idx_a_v;
    WORD32 idx_b_y, idx_a_y;

    UWORD32 u4_bs_val;

    UWORD8 *pu1_cliptab_u, *pu1_cliptab_v, *pu1_cliptab_y;

    UWORD8 u1_double_cl = !ps_cur_mb->u1_single_call;
    WORD32 ofst_a = ps_cur_mb->i1_slice_alpha_c0_offset;
    WORD32 ofst_b = ps_cur_mb->i1_slice_beta_offset;

    PROFILE_DISABLE_DEBLK()

    pu1_y = ps_tfr_cxt->pu1_mb_y;
    pu1_u = ps_tfr_cxt->pu1_mb_u;
    pu1_v = ps_tfr_cxt->pu1_mb_v;

    /* LUMA values */
    /* Deblock rounding change */
    uc_tmp = (UWORD8)((ps_left_mb->u1_mb_qp + ps_cur_mb->u1_mb_qp + 1) >> 1);
    qp_avg = uc_tmp;
    idx_a_y = qp_avg + ofst_a;
    alpha_y = gau1_ih264d_alpha_table[12 + idx_a_y];
    idx_b_y = qp_avg + ofst_b;
    beta_y = gau1_ih264d_beta_table[12 + idx_b_y];

    /* Chroma cb values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_left_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }
    idx_a_u = qp_avg + ofst_a;
    alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
    idx_b_u = qp_avg + ofst_b;
    beta_u = gau1_ih264d_beta_table[12 + idx_b_u];

    /* Chroma cr values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_left_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }
    idx_a_v = qp_avg + ofst_a;
    alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
    idx_b_v = qp_avg + ofst_b;
    beta_v = gau1_ih264d_beta_table[12 + idx_b_v];

    if(u1_double_cl == 0)
    {
        u4_bs_val = pu4_bs_tab[4];

        if(0x04040404 == u4_bs_val)
        {
            ps_dec->pf_deblk_luma_vert_bs4(pu1_y, i4_strd_y, alpha_y, beta_y);
            ps_dec->pf_deblk_chroma_vert_bs4(pu1_u, i4_strd_uv, alpha_u,
                                             beta_u, alpha_v, beta_v);

        }
        else
        {
            if(u4_bs_val)
            {

                pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12
                                + idx_a_y];
                pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12
                                + idx_a_u];
                pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12
                                + idx_a_v];

                ps_dec->pf_deblk_luma_vert_bslt4(pu1_y, i4_strd_y, alpha_y,
                                                 beta_y, u4_bs_val,
                                                 pu1_cliptab_y);
                ps_dec->pf_deblk_chroma_vert_bslt4(pu1_u, i4_strd_uv, alpha_u,
                                                   beta_u, alpha_v, beta_v,
                                                   u4_bs_val, pu1_cliptab_u,
                                                   pu1_cliptab_v);

            }
        }

    }
    else
    {

        i4_strd_y <<= (!u1_cur_fld);
        u4_bs_val = pu4_bs_tab[4];
        i4_strd_uv <<= (!u1_cur_fld);

        if(0x04040404 == u4_bs_val)
        {
            ps_dec->pf_deblk_luma_vert_bs4_mbaff(pu1_y, i4_strd_y, alpha_y,
                                                 beta_y);
            ps_dec->pf_deblk_chroma_vert_bs4_mbaff(pu1_u, i4_strd_uv, alpha_u,
                                                   beta_u, alpha_v, beta_v);
        }
        else
        {
            if(u4_bs_val)
            {

                pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y];
                pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u];
                pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v];
                ps_dec->pf_deblk_luma_vert_bslt4_mbaff(pu1_y, i4_strd_y,
                                                       alpha_y, beta_y,
                                                       u4_bs_val,
                                                       pu1_cliptab_y);
                ps_dec->pf_deblk_chroma_vert_bslt4_mbaff(pu1_u, i4_strd_uv,
                                                         alpha_u, beta_u,
                                                         alpha_v, beta_v,
                                                         u4_bs_val,
                                                         pu1_cliptab_u,
                                                         pu1_cliptab_v);

            }
        }

        {

            UWORD16 u2_shift = (i4_strd_y >> 1) << (u1_cur_fld ? 4 : 0);
            pu1_y += u2_shift;
            u2_shift = (i4_strd_uv >> 1) << (u1_cur_fld ? 3 : 0);
            pu1_u += u2_shift;
            pu1_v += u2_shift;
        }

        uc_tmp = (((ps_left_mb + 1)->u1_mb_qp + ps_cur_mb->u1_mb_qp + 1) >> 1);
        qp_avg = uc_tmp;
        idx_a_y = qp_avg + ofst_a;
        alpha_y = gau1_ih264d_alpha_table[12 + idx_a_y];
        idx_b_y = qp_avg + ofst_b;
        beta_y = gau1_ih264d_beta_table[12 + idx_b_y];
        u4_bs_val = pu4_bs_tab[9];

        {
            WORD32 mb_qp1, mb_qp2;
            mb_qp1 = ((ps_left_mb + 1)->u1_mb_qp + i1_cb_qp_idx_ofst);
            mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
            qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                            + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
        }
        idx_a_u = qp_avg + ofst_a;
        alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
        idx_b_u = qp_avg + ofst_b;
        beta_u = gau1_ih264d_beta_table[12 + idx_b_u];
        u4_bs_val = pu4_bs_tab[9];
        {
            WORD32 mb_qp1, mb_qp2;
            mb_qp1 = ((ps_left_mb + 1)->u1_mb_qp + i1_cr_qp_idx_ofst);
            mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
            qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                            + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
        }
        idx_a_v = qp_avg + ofst_a;
        alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
        idx_b_v = qp_avg + ofst_b;
        beta_v = gau1_ih264d_beta_table[12 + idx_b_v];

        if(0x04040404 == u4_bs_val)
        {
            ps_dec->pf_deblk_luma_vert_bs4_mbaff(pu1_y, i4_strd_y, alpha_y,
                                                 beta_y);
            ps_dec->pf_deblk_chroma_vert_bs4_mbaff(pu1_u, i4_strd_uv, alpha_u,
                                                   beta_u, alpha_v, beta_v);

        }
        else
        {
            if(u4_bs_val)
            {

                pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y];
                pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u];
                pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v];

                ps_dec->pf_deblk_luma_vert_bslt4_mbaff(pu1_y, i4_strd_y,
                                                       alpha_y, beta_y,
                                                       u4_bs_val,
                                                       pu1_cliptab_y);
                ps_dec->pf_deblk_chroma_vert_bslt4_mbaff(pu1_u, i4_strd_uv,
                                                         alpha_u, beta_u,
                                                         alpha_v, beta_v,
                                                         u4_bs_val,
                                                         pu1_cliptab_u,
                                                         pu1_cliptab_v);

            }
        }
    }

}

void ih264d_filter_boundary_topmbaff(dec_struct_t *ps_dec,
                                     tfr_ctxt_t * ps_tfr_cxt,
                                     WORD8 i1_cb_qp_idx_ofst,
                                     WORD8 i1_cr_qp_idx_ofst,
                                     deblk_mb_t * ps_cur_mb,
                                     WORD32 i4_strd_y,
                                     WORD32 i4_strd_uv,
                                     deblk_mb_t * ps_top_mb,
                                     UWORD32 u4_bs)
{
    UWORD8 *pu1_y, *pu1_u;
    WORD32 alpha_u = 0, beta_u = 0, alpha_v = 0, beta_v = 0;
    WORD32 alpha_y = 0, beta_y = 0;
    WORD32 qp_avg;
    WORD32 idx_b_u, idx_a_u, idx_b_v, idx_a_v;
    WORD32 idx_b_y, idx_a_y;
    UWORD16 uc_tmp;

    UWORD8 *pu1_cliptab_u, *pu1_cliptab_v, *pu1_cliptab_y;
    WORD32 ofst_a = ps_cur_mb->i1_slice_alpha_c0_offset;
    WORD32 ofst_b = ps_cur_mb->i1_slice_beta_offset;

    /* LUMA values */
    /* Deblock rounding change */
    uc_tmp = ((ps_top_mb->u1_mb_qp + ps_cur_mb->u1_mb_qp + 1) >> 1);
    qp_avg = (UWORD8)uc_tmp;
    idx_a_y = qp_avg + ofst_a;
    alpha_y = gau1_ih264d_alpha_table[12 + idx_a_y];
    idx_b_y = qp_avg + ofst_b;
    beta_y = gau1_ih264d_beta_table[12 + idx_b_y];
    pu1_y = ps_tfr_cxt->pu1_mb_y;

    /* CHROMA cb values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_top_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cb_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }

    idx_a_u = qp_avg + ofst_a;
    alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
    idx_b_u = qp_avg + ofst_b;
    beta_u = gau1_ih264d_beta_table[12 + idx_b_u];
    /* CHROMA cr values */
    {
        WORD32 mb_qp1, mb_qp2;
        mb_qp1 = (ps_top_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
        mb_qp2 = (ps_cur_mb->u1_mb_qp + i1_cr_qp_idx_ofst);
        qp_avg = (UWORD8)((gau1_ih264d_qp_scale_cr[12 + mb_qp1]
                        + gau1_ih264d_qp_scale_cr[12 + mb_qp2] + 1) >> 1);
    }

    idx_a_v = qp_avg + ofst_a;
    alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
    idx_b_v = qp_avg + ofst_b;
    beta_v = gau1_ih264d_beta_table[12 + idx_b_v];
    pu1_u = ps_tfr_cxt->pu1_mb_u;

    if(u4_bs == 0x04040404)
    {
        /* Code specific to the assembly module */
        ps_dec->pf_deblk_luma_horz_bs4(pu1_y, i4_strd_y, alpha_y, beta_y);
        ps_dec->pf_deblk_chroma_horz_bs4(pu1_u, i4_strd_uv, alpha_u, beta_u,
                                         alpha_v, beta_v);

    }
    else
    {
        if(u4_bs)
        {

            pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y];
            pu1_cliptab_u =
                            (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u];
            pu1_cliptab_v =
                            (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v];

            ps_dec->pf_deblk_luma_horz_bslt4(pu1_y, i4_strd_y, alpha_y, beta_y,
                                             u4_bs, pu1_cliptab_y);
            ps_dec->pf_deblk_chroma_horz_bslt4(pu1_u, i4_strd_uv, alpha_u,
                                               beta_u, alpha_v, beta_v,
                                               u4_bs, pu1_cliptab_u,
                                               pu1_cliptab_v);

        }
    }

}

void ih264d_deblock_mb_mbaff(dec_struct_t *ps_dec,
                             tfr_ctxt_t * ps_tfr_cxt,
                             WORD8 i1_cb_qp_idx_ofst,
                             WORD8 i1_cr_qp_idx_ofst,
                             deblk_mb_t * ps_cur_mb,
                             WORD32 i4_strd_y,
                             WORD32 i4_strd_uv,
                             deblk_mb_t * ps_top_mb,
                             deblk_mb_t * ps_left_mb,
                             UWORD8 u1_cur_fld,
                             UWORD8 u1_extra_top_edge)
{
    UWORD8 *pu1_y, *pu1_u;
    UWORD32 u4_bs;
//  WORD8  edge;
    WORD32 alpha, beta, alpha_u, beta_u, alpha_v, beta_v;

    UWORD8 *pu1_cliptab_u;
    UWORD8 *pu1_cliptab_v;
    UWORD8 *pu1_cliptab_y;

    UWORD32 * pu4_bs_tab = ps_cur_mb->u4_bs_table;
    WORD32 idx_a_y, idx_a_u, idx_a_v;
    /* Return from here to switch off deblocking */
    PROFILE_DISABLE_DEBLK()

    i4_strd_y <<= u1_cur_fld;
    i4_strd_uv <<= u1_cur_fld;
    /*--------------------------------------------------------------------*/
    /* Filter wrt Left edge                                               */
    /* except                                                             */
    /*      - Left Egde is Picture Boundary                               */
    /*      - Left Egde is part of Slice Boundary and Deblocking          */
    /*        parameters of slice disable Filtering of Slice Boundary Edges*/
    /*--------------------------------------------------------------------*/
    if(ps_left_mb)
        ih264d_filter_boundary_left_mbaff(ps_dec, ps_tfr_cxt, i1_cb_qp_idx_ofst,
                                          i1_cr_qp_idx_ofst, ps_cur_mb,
                                          i4_strd_y, i4_strd_uv, ps_left_mb,
                                          pu4_bs_tab, u1_cur_fld);

    /*--------------------------------------------------------------------*/
    /* Filter wrt Other Vertical Edges                                    */
    /*--------------------------------------------------------------------*/
    {
        WORD32 ofst_a, ofst_b, idx_b_y, idx_b_u,
                        idx_b_v;
        WORD32 qp_avg, qp_avg_u, qp_avg_v;
        ofst_a = ps_cur_mb->i1_slice_alpha_c0_offset;
        ofst_b = ps_cur_mb->i1_slice_beta_offset;
        qp_avg = ps_cur_mb->u1_mb_qp;
        idx_a_y = qp_avg + ofst_a;
        alpha = gau1_ih264d_alpha_table[12 + idx_a_y];
        idx_b_y = qp_avg + ofst_b;
        beta = gau1_ih264d_beta_table[12 + idx_b_y];

        /* CHROMA Cb values */
        qp_avg_u = (qp_avg + i1_cb_qp_idx_ofst);
        qp_avg_u = gau1_ih264d_qp_scale_cr[12 + qp_avg_u];
        idx_a_u = qp_avg_u + ofst_a;
        alpha_u = gau1_ih264d_alpha_table[12 + idx_a_u];
        idx_b_u = qp_avg_u + ofst_b;
        beta_u = gau1_ih264d_beta_table[12 + idx_b_u];
        /* CHROMA Cr values */
        qp_avg_v = (qp_avg + i1_cr_qp_idx_ofst);
        qp_avg_v = gau1_ih264d_qp_scale_cr[12 + qp_avg_v];
        idx_a_v = qp_avg_v + ofst_a;
        alpha_v = gau1_ih264d_alpha_table[12 + idx_a_v];
        idx_b_v = qp_avg_v + ofst_b;
        beta_v = gau1_ih264d_beta_table[12 + idx_b_v];
    }

    //STARTL4_FILTER_VERT;

    pu1_cliptab_y = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_y]; //this for Luma
    pu1_cliptab_u = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_u]; //this for chroma
    pu1_cliptab_v = (UWORD8 *)&gau1_ih264d_clip_table[12 + idx_a_v]; //this for chroma

    //edge=1


    u4_bs = pu4_bs_tab[5];
    pu1_y = ps_tfr_cxt->pu1_mb_y;
    pu1_u = ps_tfr_cxt->pu1_mb_u;

    if(u4_bs)
    {

        ps_dec->pf_deblk_luma_vert_bslt4(pu1_y + 4, i4_strd_y, alpha, beta,
                                         u4_bs, pu1_cliptab_y);

    }
    //edge=2

    u4_bs = pu4_bs_tab[6];
    if(u4_bs)
    {

        ps_dec->pf_deblk_luma_vert_bslt4(pu1_y + 8, i4_strd_y, alpha, beta,
                                         u4_bs, pu1_cliptab_y);
        ps_dec->pf_deblk_chroma_vert_bslt4(pu1_u + 4 * YUV420SP_FACTOR,
                                           i4_strd_uv, alpha_u, beta_u,
                                           alpha_v, beta_v, u4_bs,
                                           pu1_cliptab_u, pu1_cliptab_v);
    }
    //edge=3

    u4_bs = pu4_bs_tab[7];
    if(u4_bs)
    {

        ps_dec->pf_deblk_luma_vert_bslt4(pu1_y + 12, i4_strd_y, alpha, beta,
                                         u4_bs, pu1_cliptab_y);

    }

    /*--------------------------------------------------------------------*/
    /* Filter wrt Top edge                                                */
    /* except                                                             */
    /*      - Top Egde is Picture Boundary                                */
    /*      - Top Egde is part of Slice Boundary and Deblocking           */
    /*        parameters of slice disable Filtering of Slice Boundary Edges*/
    /*--------------------------------------------------------------------*/
    if(ps_top_mb)
    {
        /** if top MB and MB AFF and cur MB is frame and top is field then  */
        /*  one extra top edge needs to be deblocked                        */
        if(u1_extra_top_edge)
        {
            ih264d_filter_boundary_topmbaff(ps_dec, ps_tfr_cxt,
                                            i1_cb_qp_idx_ofst,
                                            i1_cr_qp_idx_ofst, ps_cur_mb,
                                            (UWORD16)(i4_strd_y << 1),
                                            (UWORD16)(i4_strd_uv << 1),
                                            ps_top_mb - 1, pu4_bs_tab[8]);
            ps_tfr_cxt->pu1_mb_y += i4_strd_y;
            ps_tfr_cxt->pu1_mb_u += i4_strd_uv;
            ps_tfr_cxt->pu1_mb_v += i4_strd_uv;

            ih264d_filter_boundary_topmbaff(ps_dec, ps_tfr_cxt,
                                            i1_cb_qp_idx_ofst,
                                            i1_cr_qp_idx_ofst, ps_cur_mb,
                                            (UWORD16)(i4_strd_y << 1),
                                            (UWORD16)(i4_strd_uv << 1),
                                            ps_top_mb, pu4_bs_tab[0]);
            ps_tfr_cxt->pu1_mb_y -= i4_strd_y;
            ps_tfr_cxt->pu1_mb_u -= i4_strd_uv;
            ps_tfr_cxt->pu1_mb_v -= i4_strd_uv;
        }
        else
        {
            ih264d_filter_boundary_topmbaff(ps_dec, ps_tfr_cxt,
                                            i1_cb_qp_idx_ofst,
                                            i1_cr_qp_idx_ofst, ps_cur_mb,
                                            i4_strd_y, i4_strd_uv, ps_top_mb,
                                            pu4_bs_tab[0]);
        }
    }

    /*--------------------------------------------------------------------*/
    /* Filter wrt Other Horizontal Edges                                  */
    /*--------------------------------------------------------------------*/

    //edge1
    u4_bs = pu4_bs_tab[1];

    if(u4_bs)
    {
        ps_dec->pf_deblk_luma_horz_bslt4(pu1_y + (i4_strd_y << 2), i4_strd_y,
                                         alpha, beta, u4_bs, pu1_cliptab_y);

    }
    //edge2
    u4_bs = pu4_bs_tab[2];

    if(u4_bs)
    {

        ps_dec->pf_deblk_luma_horz_bslt4(pu1_y + (i4_strd_y << 3), i4_strd_y,
                                         alpha, beta, u4_bs, pu1_cliptab_y);
        ps_dec->pf_deblk_chroma_horz_bslt4(pu1_u + (i4_strd_uv << 2),
                                           i4_strd_uv, alpha_u, beta_u,
                                           alpha_v, beta_v, u4_bs,
                                           pu1_cliptab_u, pu1_cliptab_v);

    }
    //edge3
    u4_bs = pu4_bs_tab[3];
    if(u4_bs)
    {

        ps_dec->pf_deblk_luma_horz_bslt4(
                        (pu1_y + (i4_strd_y << 3) + (i4_strd_y << 2)),
                        i4_strd_y, alpha, beta, u4_bs, pu1_cliptab_y);

    }

}

