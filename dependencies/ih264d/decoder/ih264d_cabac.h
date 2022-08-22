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
 * \file ih264d_cabac.h
 *
 * \brief
 *    This file contains declarations of Binary decoding routines and tables.
 *
 * \date
 *    04/02/2003
 *
 * \author  NS
 ***************************************************************************
 */

#ifndef _IH264D_CABAC_H_
#define _IH264D_CABAC_H_

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"
#include "ih264d_defs.h"

#define   B_BITS    10

#define   HALF      (1 << (B_BITS-1))
#define   QUARTER   (1 << (B_BITS-2))

#define CTXT_UNUSED   {0,64}
#define NUM_MB_SKIP_CTXT  6
#define NUM_MB_TYPE_CTXT  9
#define NUM_SUBMB_TYPE_CTXT 7
#define NUM_REF_IDX_CTXT  6
#define NUM_MB_QP_DELTA 4
#define NUM_PRED_MODE 6
#define NUM_MB_FIELD    3
#define NUM_CBP 12
#define NUM_CTX_MVD 14

/* Residual block cabac context parameters */
#define NUM_CTX_CAT 6
#define NUM_LUMA_CTX_CAT 3
#define NUM_CTX_CODED_BLOCK 4
/* Luma CtxSigCoeff + CtxLastCoeff = 15 + 15 = 30 */
#define NUM_LUMA_CTX_SIG_COEF 30
/* Chroma DC CtxSigCoeff + CtxLastCoeff = 3 + 3 = 6 */
#define NUM_CTX_CHROMA_DC_SIG_COEF 6
/* Chroma AC CtxSigCoeff + CtxLastCoeff = 14 + 14 = 28 */
#define NUM_CTX_CHROMA_AC_SIG_COEF 28
#define NUM_CTX_ABS_LEVEL 10

#define LUMA_DC_CTXCAT    0
#define LUMA_AC_CTXCAT    1
#define LUMA_4X4_CTXCAT   2
#define CHROMA_DC_CTXCAT  3
#define CHROMA_AC_CTXCAT  4
#define LUMA_8X8_CTXCAT   5

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define NUM_CABAC_CTXTS 460
#define QP_RANGE        52
#define NUM_CAB_INIT_IDC_PLUS_ONE 4
#define LAST_COEFF_CTXT_MINUS_SIG_COEFF_CTXT 61
#define LAST_COEFF_CTXT_MINUS_SIG_COEFF_CTXT_8X8 15

/*bits 0 to 5 :state
 bit 6:mps*/
typedef struct
{
    UWORD8 u1_mps_state; /* state number */
} bin_ctxt_model_t;

typedef struct

{
    /* Neighbour availability Variables needed to get CtxtInc, for CABAC */
    UWORD8 u1_mb_type; /** macroblock type: I/P/B/SI/SP */
    UWORD8 u1_cbp; /** Coded Block Pattern */
    UWORD8 u1_intra_chroma_pred_mode;

    /*************************************************************************/
    /*               Arrangnment of DC CSBP                                  */
    /*        bits:  b7  b6  b5  b4  b3  b2  b1  b0                          */
    /*        CSBP:   x   x   x   x   x  Vdc Udc Ydc                         */
    /*************************************************************************/
    UWORD8 u1_yuv_dc_csbp;
    WORD8 i1_ref_idx[4];
    UWORD8 u1_mv[4][4];
    UWORD8 u1_transform8x8_ctxt;
} ctxt_inc_mb_info_t;

#define ONE_RIGHT_SHIFTED_BY_8 1<<8
#define ONE_RIGHT_SHIFTED_BY_9    1<<9
#define ONE_RIGHT_SHIFTED_BY_14 1<<14
typedef struct
{
    UWORD32 u4_code_int_range;
    UWORD32 u4_code_int_val_ofst;
    const void *cabac_table;
    void * pv_codec_handle; /* For Error Handling */
} decoding_envirnoment_t;

WORD32 ih264d_init_cabac_dec_envirnoment(decoding_envirnoment_t * ps_cab_env,
                                       dec_bit_stream_t *ps_bitstrm);

UWORD32 ih264d_decode_bin(UWORD32 u4_ctx_inc,
                          bin_ctxt_model_t *ps_bin_ctxt,
                          dec_bit_stream_t *ps_bitstrm,
                          decoding_envirnoment_t *ps_cab_env);
UWORD8 ih264d_decode_terminate(decoding_envirnoment_t * ps_cab_env,
                               dec_bit_stream_t * ps_bitstrm);

UWORD32 ih264d_decode_bins_tunary(UWORD8 u1_max_bins,
                                  UWORD32 u4_ctx_inc,
                                  bin_ctxt_model_t *ps_src_bin_ctxt,
                                  dec_bit_stream_t *ps_bitstrm,
                                  decoding_envirnoment_t *ps_cab_env);

UWORD32 ih264d_decode_bins(UWORD8 u1_max_bins,
                           UWORD32 u4_ctx_inc,
                           bin_ctxt_model_t *ps_src_bin_ctxt,
                           dec_bit_stream_t *ps_bitstrm,
                           decoding_envirnoment_t *ps_cab_env);
UWORD32 ih264d_decode_bins_unary(UWORD8 u1_max_bins,
                                 UWORD32 u4_ctx_inc,
                                 bin_ctxt_model_t *ps_src_bin_ctxt,
                                 dec_bit_stream_t *ps_bitstrm,
                                 decoding_envirnoment_t *ps_cab_env);

UWORD32 ih264d_decode_bypass_bins_unary(decoding_envirnoment_t *ps_cab_env,
                                        dec_bit_stream_t *ps_bitstrm);

UWORD32 ih264d_decode_bypass_bins(decoding_envirnoment_t *ps_cab_env,
                                  UWORD8 u1_max_bins,
                                  dec_bit_stream_t *ps_bitstrm);

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Defining a macro for renormalization*/
/*****************************************************************************/

/*we renormalize every time the number bits(which are read ahead of time) we have
 consumed in the u4_ofst exceeds 23*/

#define RENORM_RANGE_OFFSET(u4_codeIntRange_m,u4_codeIntValOffset_m,u4_offset_m,pu4_buffer_m) \
  {                                                                                         \
    UWORD32 read_bits_m,u4_clz_m  ;                                                         \
    u4_clz_m = CLZ(u4_codeIntRange_m);                                                  \
    NEXTBITS(read_bits_m,(u4_offset_m+23),pu4_buffer_m,u4_clz_m)                            \
    FLUSHBITS(u4_offset_m,(u4_clz_m))                                                       \
    u4_codeIntRange_m = u4_codeIntRange_m << u4_clz_m;                                      \
    u4_codeIntValOffset_m = (u4_codeIntValOffset_m << u4_clz_m) | read_bits_m;              \
  }

/*****************************************************************************/
/* Defining a macro for checking if the symbol is MPS*/
/*****************************************************************************/

#define CHECK_IF_LPS(u4_codeIntRange_m,u4_codeIntValOffset_m,u4_symbol_m,                   \
                    u4_codeIntRangeLPS_m,u1_mps_state_m,table_lookup_m)                     \
{                                                                                         \
  if(u4_codeIntValOffset_m >= u4_codeIntRange_m)                                            \
  {                                                                                         \
      u4_symbol_m = 1 - u4_symbol_m;                                                        \
      u4_codeIntValOffset_m -= u4_codeIntRange_m;                                           \
      u4_codeIntRange_m = u4_codeIntRangeLPS_m;                                             \
      u1_mps_state_m = (table_lookup_m >> 15) & 0x7F;                                       \
  }                                                                                         \
}

/*!
 **************************************************************************
 * \if Function name : DECODE_ONE_BIN_MACRO \endif
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

#define DECODE_ONE_BIN_MACRO(p_binCtxt_arg ,u4_code_int_range,u4_code_int_val_ofst,       \
                     pu4_table_arg,                                                \
                     p_DecBitStream_arg,u4_symbol)                                           \
{                                                                                       \
    bin_ctxt_model_t *p_binCtxt_m = (bin_ctxt_model_t *) p_binCtxt_arg;                           \
    dec_bit_stream_t *p_DecBitStream_m = (dec_bit_stream_t *) p_DecBitStream_arg;                 \
    const UWORD32 *pu4_table_m = (const UWORD32 *) pu4_table_arg;                         \
                                                                                        \
    UWORD32 u4_quantCodeIntRange_m,u4_codeIntRangeLPS_m;                                    \
    UWORD32 u1_mps_state_m;                                                               \
    UWORD32 table_lookup_m;                                                               \
    UWORD32 u4_clz_m;                                                                     \
                                                                                        \
    u1_mps_state_m = (p_binCtxt_m->u1_mps_state);                                           \
    u4_clz_m = CLZ(u4_code_int_range);                                                  \
    u4_quantCodeIntRange_m = u4_code_int_range << u4_clz_m;                                   \
    u4_quantCodeIntRange_m = (u4_quantCodeIntRange_m >> 29) & 0x3;                          \
    table_lookup_m = pu4_table_m[(u1_mps_state_m << 2)+u4_quantCodeIntRange_m];                 \
    u4_codeIntRangeLPS_m = table_lookup_m & 0xff;                                           \
                                                                                        \
    u4_codeIntRangeLPS_m = u4_codeIntRangeLPS_m << (23 - u4_clz_m);                           \
    u4_code_int_range = u4_code_int_range - u4_codeIntRangeLPS_m;                             \
    u4_symbol = ((u1_mps_state_m>> 6) & 0x1);                                             \
    /*if mps*/                                                                          \
    u1_mps_state_m = (table_lookup_m >> 8) & 0x7F;                                          \
    if(u4_code_int_val_ofst >= u4_code_int_range)                                          \
  {                                                                                     \
                                                                                        \
    u4_symbol = 1 - u4_symbol;                                                          \
    u4_code_int_val_ofst -= u4_code_int_range;                                             \
    u4_code_int_range = u4_codeIntRangeLPS_m;                                               \
    u1_mps_state_m = (table_lookup_m >> 15) & 0x7F;                                         \
  }                                                                                     \
    if(u4_code_int_range < ONE_RIGHT_SHIFTED_BY_8)                                        \
    {                                                                                   \
        UWORD32 *pu4_buffer,u4_offset;                                                  \
        UWORD32 read_bits,u4_clz_m  ;                                                     \
                                                                                        \
        pu4_buffer = p_DecBitStream_m->pu4_buffer;                                         \
        u4_offset = p_DecBitStream_m->u4_ofst;                                          \
        u4_clz_m = CLZ(u4_code_int_range);                                              \
        NEXTBITS(read_bits,(u4_offset+23),pu4_buffer,u4_clz_m)                            \
        FLUSHBITS(u4_offset,(u4_clz_m))                                                   \
        u4_code_int_range = u4_code_int_range << u4_clz_m;                                    \
        u4_code_int_val_ofst= (u4_code_int_val_ofst << u4_clz_m) | read_bits;               \
                                                                                        \
                                                                                        \
        p_DecBitStream_m->u4_ofst = u4_offset;                                          \
    }                                                                                   \
    p_binCtxt_m->u1_mps_state = u1_mps_state_m;                                             \
}

#endif  /*  _IH264D_CABAC_H_ */
