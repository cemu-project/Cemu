//******************************************************************************
//*
//* Copyright (C) 2015 The Android Open Source Project
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at:
//*
//* http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.
//*
//*****************************************************************************
//* Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
//*/
///**
// *******************************************************************************
// * @file
// *  ih264_iquant_itrans_recon_dc_av8.s
// *
// * @brief
// *  Contains function definitions for single stage  inverse transform
// *
// * @author
// *  Mohit
// *
// * @par List of Functions:
// *  - ih264_iquant_itrans_recon_4x4_dc_av8()
// *     - ih264_iquant_itrans_recon_8x8_dc_av8()
// *  - ih264_iquant_itrans_recon_chroma_4x4_dc_av8()
// *
// * @remarks
// *  None
// *
// *******************************************************************************
//*/


.include "ih264_neon_macros.s"


///**
// *******************************************************************************
// *
// * @brief
// *  This function performs inverse quant and Inverse transform type Ci4 for 4*4 block
// *     for dc input pattern only, i.e. only the (0,0) element of the input 4x4 block is
// *  non-zero. For complete function, refer ih264_iquant_itrans_recon_a9.s
// *
// * @par Description:
// *  Performs inverse transform Ci4 and adds the residue to get the
// *  reconstructed block
// *
// * @param[in] pi2_src
// *  Input 4x4 coefficients
// *
// * @param[in] pu1_pred
// *  Prediction 4x4 block
// *
// * @param[out] pu1_out
// *  Output 4x4 block
// *
// * @param[in] u4_qp_div_6
// *     QP
// *
// * @param[in] pu2_weigh_mat
// * Pointer to weight matrix
// *
// * @param[in] pred_strd,
// *  Prediction stride
// *
// * @param[in] out_strd
// *  Output Stride
// *
// *@param[in] pi2_tmp
// * temporary buffer of size 1*16
// *
// * @param[in] pu2_iscal_mat
// * Pointer to the inverse quantization matrix
// *
// * @returns  Void
// *
// * @remarks
// *  None
// *
// *******************************************************************************
// */
//void ih264_iquant_itrans_recon_4x4_dc(WORD16 *pi2_src,
//                                    UWORD8 *pu1_pred,
//                                    UWORD8 *pu1_out,
//                                    WORD32 pred_strd,
//                                    WORD32 out_strd,
//                                    const UWORD16 *pu2_iscal_mat,
//                                    const UWORD16 *pu2_weigh_mat,
//                                    UWORD32 u4_qp_div_6,
//                                    WORD32 *pi4_tmp,
//                                    WORD32 iq_start_idx
//                                   WORD16 *pi2_dc_ld_addr)
//**************Variables Vs Registers*****************************************
//x0 => *pi2_src
//x1 => *pu1_pred
//x2 => *pu1_out
//w3 =>  pred_strd
//w4 =>  out_strd
//x5 => *pu2_iscal_mat
//x6 => *pu2_weigh_mat
//w7 =>  u4_qp_div_6
//   =>  pi4_tmp
//   =>  iq_start_idx
//   =>  pi2_dc_ld_addr

.text
.p2align 2

    .global ih264_iquant_itrans_recon_4x4_dc_av8
ih264_iquant_itrans_recon_4x4_dc_av8:

    sxtw      x3, w3
    sxtw      x4, w4
    ldr       w8, [sp, #8]              //Loads iq_start_idx
    subs      w8, w8, #1                // if x8 == 1 => intra case , so result of subtraction is zero and z flag is set

    ldr       x10, [sp, #16]            //Load alternate dc address
    push_v_regs
    dup       v30.4s, w7                //Populate the u4_qp_div_6 in Q15


    bne       donot_use_pi2_dc_ld_addr_luma_dc
    ld1       {v0.h}[0], [x10]
donot_use_pi2_dc_ld_addr_luma_dc:

    beq       donot_use_pi2_src_luma_dc
    ld1       {v0.h}[0], [x5]
    ld1       {v1.h}[0], [x6]
    ld1       {v2.h}[0], [x0]
    mul       v0.4h, v1.4h, v0.4h
    smull     v0.4s, v0.4h, v2.4h
    sshl      v0.4s, v0.4s, v30.4s
    sqrshrn   v0.4h, v0.4s, #4
donot_use_pi2_src_luma_dc:


    dup       v0.8h, v0.h[0]
    srshr     v0.8h, v0.8h, #6

    ld1       {v1.s}[0], [x1], x3
    ld1       {v1.s}[1], [x1], x3
    ld1       {v2.s}[0], [x1], x3
    ld1       {v2.s}[1], [x1]

    uxtl      v1.8h, v1.8b
    uxtl      v2.8h, v2.8b

    add       v1.8h, v0.8h, v1.8h
    add       v2.8h, v0.8h, v2.8h

    sqxtun    v1.8b, v1.8h
    sqxtun    v2.8b, v2.8h

    st1       {v1.s}[0], [x2], x4
    st1       {v1.s}[1], [x2], x4
    st1       {v2.s}[0], [x2], x4
    st1       {v2.s}[1], [x2]
    pop_v_regs
    ret

// /*
// ********************************************************************************
// *
// * @brief This function reconstructs a 4x4 sub block from quantized resiude and
// * prediction buffer if only dc value is present for residue
// *
// * @par Description:
// *  The quantized residue is first inverse quantized,
// *  This inverse quantized content is added to the prediction buffer to recon-
// *  struct the end output
// *
// * @param[in] pi2_src
// *  quantized dc coeffiient
// *
// * @param[in] pu1_pred
// *  prediction 4x4 block in interleaved format
// *
// * @param[in] pred_strd,
// *  Prediction buffer stride in interleaved format
// *
// * @param[in] out_strd
// *  recon buffer Stride
// *
// * @returns none
// *
// * @remarks none
// *
// *******************************************************************************
// */
// void ih264_iquant_itrans_recon_chroma_4x4_dc(WORD16 *pi2_src,
//                                             UWORD8 *pu1_pred,
//                                             UWORD8 *pu1_out,
//                                             WORD32 pred_strd,
//                                             WORD32 out_strd,
//                                             const UWORD16 *pu2_iscal_mat,
//                                             const UWORD16 *pu2_weigh_mat,
//                                             UWORD32 u4_qp_div_6,
//                                             WORD16 *pi2_tmp,
//                                             WORD16 *pi2_dc_src)
// Register Usage
// x0 : pi2_src
// x1 : pu1_pred
// x2 : pu1_out
// w3 : pred_strd
// w4 : out_strd
// x5 : pu2_iscal_mat
// x6 : pu2_weigh_mat
// w7 : u4_qp_div_6
//    : pi2_tmp
//    : pi2_dc_src
// Neon registers d0-d7, d16-d30 are used
// No need for pushing  arm and neon registers


    .global ih264_iquant_itrans_recon_chroma_4x4_dc_av8
ih264_iquant_itrans_recon_chroma_4x4_dc_av8:

    sxtw      x3, w3
    sxtw      x4, w4
    ldr       x0, [sp, #8]
    push_v_regs
    ld1       {v0.h}[0], [x0]
    dup       v0.8h, v0.h[0]
    srshr     v0.8h, v0.8h, #6


    //backup pu1_out
    mov       x0, x2

    //nop       v3.16b                            //dummy for deinterleaving
    movi      v31.8h, #0x00ff           //mask for interleaving [copy lower 8 bits]

    ld1       {v1.d}[0], [x1], x3
    ld1       {v1.d}[1], [x1], x3
    ld1       {v2.d}[0], [x1], x3
    ld1       {v2.d}[1], [x1], x3

    ld1       {v11.d}[0], [x2], x4      //load pu1_out for interleaving
    ld1       {v11.d}[1], [x2], x4
    ld1       {v12.d}[0], [x2], x4
    ld1       {v12.d}[1], [x2]

    uzp1      v1.16b, v1.16b, v3.16b
    uzp1      v2.16b, v2.16b, v3.16b

    uaddw     v1.8h, v0.8h, v1.8b
    uaddw     v2.8h, v0.8h, v2.8b

    sqxtun    v1.8b, v1.8h
    sqxtun    v2.8b, v2.8h

    uxtl      v1.8h, v1.8b
    uxtl      v2.8h, v2.8b

    bit       v11.16b, v1.16b, v31.16b
    bit       v12.16b, v2.16b, v31.16b

    st1       {v11.d}[0], [x0], x4
    st1       {v11.d}[1], [x0], x4
    st1       {v12.d}[0], [x0], x4
    st1       {v12.d}[1], [x0]
    pop_v_regs
    ret

///*
// *******************************************************************************
// *
// * //brief
// *  This function performs inverse quant and Inverse transform type Ci4 for 8*8 block
// *   [Only for Dc coeff]
// * //par Description:
// *  Performs inverse transform Ci8 and adds the residue to get the
// *  reconstructed block
// *
// * //param[in] pi2_src
// *  Input 4x4 coefficients
// *
// * //param[in] pu1_pred
// *  Prediction 4x4 block
// *
// * //param[out] pu1_out
// *  Output 4x4 block
// *
// * //param[in] u4_qp_div_6
// *     QP
// *
// * //param[in] pu2_weigh_mat
// * Pointer to weight matrix
// *
// * //param[in] pred_strd,
// *  Prediction stride
// *
// * //param[in] out_strd
// *  Output Stride
// *
// *//param[in] pi2_tmp
// * temporary buffer of size 1*64
// *
// * //param[in] pu2_iscal_mat
// * Pointer to the inverse quantization matrix
// *
// * //returns  Void
// *
// * //remarks
// *  None
// *
// *******************************************************************************
// */
//void ih264_iquant_itrans_recon_dc_8x8(WORD16 *pi2_src,
//                                   UWORD8 *pu1_pred,
//                                   UWORD8 *pu1_out,
//                                   WORD32 pred_strd,
//                                   WORD32 out_strd,
//                                   const UWORD16 *pu2_iscal_mat,
//                                   const UWORD16 *pu2_weigh_mat,
//                                   UWORD32 u4_qp_div_6,
//                                   WORD32 *pi4_tmp,
//                                   WORD32 iq_start_idx
//                                   WORD16 *pi2_dc_ld_addr)
//**************Variables Vs Registers*****************************************
//x0       => *pi2_src
//x1       => *pu1_pred
//x2       => *pu1_out
//w3       =>  pred_strd
//w4       =>  out_strd
//x5       =>  *pu2_iscal_mat
//x6       =>  *pu2_weigh_mat
//w7       =>  u4_qp_div_6
//NOT USED =>  pi4_tmp
//NOT USED =>  iq_start_idx
//NOT USED =>  pi2_dc_ld_addr

    .global ih264_iquant_itrans_recon_8x8_dc_av8
ih264_iquant_itrans_recon_8x8_dc_av8:

    push_v_regs
    sxtw      x3, w3
    sxtw      x4, w4

    ld1       {v1.h}[0], [x5]
    ld1       {v2.h}[0], [x6]
    ld1       {v0.h}[0], [x0]
    dup       v3.4s, w7


    mul       v1.8h, v1.8h, v2.8h
    smull     v0.4s, v0.4h, v1.4h
    sshl      v0.4s, v0.4s, v3.4s

    sqrshrn   v0.4h, v0.4s, #6
    srshr     v0.8h, v0.8h, #6
    dup       v0.8h, v0.h[0]

    ld1       {v22.8b}, [x1], x3
    ld1       {v23.8b}, [x1], x3
    ld1       {v24.8b}, [x1], x3
    ld1       {v25.8b}, [x1], x3
    ld1       {v26.8b}, [x1], x3
    ld1       {v27.8b}, [x1], x3
    ld1       {v28.8b}, [x1], x3
    ld1       {v29.8b}, [x1]

    uaddw     v1.8h, v0.8h, v22.8b
    uaddw     v2.8h, v0.8h, v23.8b
    uaddw     v3.8h, v0.8h, v24.8b
    uaddw     v8.8h, v0.8h, v25.8b
    uaddw     v9.8h, v0.8h, v26.8b
    uaddw     v10.8h, v0.8h, v27.8b
    uaddw     v11.8h, v0.8h, v28.8b
    uaddw     v12.8h, v0.8h, v29.8b

    sqxtun    v1.8b, v1.8h
    sqxtun    v2.8b, v2.8h
    sqxtun    v3.8b, v3.8h
    sqxtun    v8.8b, v8.8h
    sqxtun    v9.8b, v9.8h
    sqxtun    v10.8b, v10.8h
    sqxtun    v11.8b, v11.8h
    sqxtun    v12.8b, v12.8h

    st1       {v1.8b}, [x2], x4
    st1       {v2.8b}, [x2], x4
    st1       {v3.8b}, [x2], x4
    st1       {v8.8b}, [x2], x4
    st1       {v9.8b}, [x2], x4
    st1       {v10.8b}, [x2], x4
    st1       {v11.8b}, [x2], x4
    st1       {v12.8b}, [x2]

    pop_v_regs
    ret


