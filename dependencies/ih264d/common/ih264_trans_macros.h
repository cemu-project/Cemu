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
/**
*******************************************************************************
* @file
*  ih264_trans_macros.h
*
* @brief
*  The file contains definitions of macros that perform forward and inverse
*  quantization
*
* @author
*  Ittiam
*
* @remark
*  None
*
*******************************************************************************
*/

#ifndef IH264_TRANS_MACROS_H_
#define IH264_TRANS_MACROS_H_

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief   Macro to perform forward quantization.
 *  @description The value to be quantized is first compared with a threshold.
 *  If the value is less than the threshold, the quantization value is returned
 *  as zero else the value is quantized traditionally as per the rules of
 *  h264 specification
******************************************************************************
 */
#define FWD_QUANT(i4_value, u4_abs_value, i4_sign, threshold, scale, rndfactor, qbits, u4_nnz)      \
                {\
                        if (i4_value < 0)\
                        {\
                            u4_abs_value = -i4_value;\
                            i4_sign = -1;\
                        }\
                        else\
                        {\
                            u4_abs_value = i4_value;\
                            i4_sign = 1;\
                        }\
                        if (u4_abs_value < threshold)\
                        {\
                            i4_value = 0;\
                        }\
                        else\
                        {\
                            u4_abs_value *= scale;\
                            u4_abs_value += rndfactor;\
                            u4_abs_value >>= qbits;\
                            i4_value = u4_abs_value * i4_sign;\
                            if (i4_value)\
                            {\
                                u4_nnz++;\
                            }\
                        }\
                }

/**
******************************************************************************
 *  @brief   Macro to perform inverse quantization.
 *  @remarks The value can also be de-quantized as
 *  if (u4_qp_div_6 < 4)
 *  {
 *      i4_value = (quant_scale * weight_scale * i4_value + (1 << (3-u4_qp_div_6)))
 *      i4_value >>= (4 - u4_qp_div_6)
 *  }
 *  else
 *  {
 *      i4_value = (quant_scale * weight_scale * i4_value) << (u4_qp_div_6 -4)
 *  }
******************************************************************************
 */
#define INV_QUANT(i4_value, quant_scale, weight_scale, u4_qp_div_6, rndfactor, qbits)\
                {\
                    i4_value *= quant_scale;\
                    i4_value *= weight_scale;\
                    i4_value += rndfactor;\
                    i4_value <<= u4_qp_div_6;\
                    i4_value >>= qbits;\
                }

#define QUANT_H264(x,y,w,z,shft) (shft = ABS(x),\
                shft *= y,\
                shft += z,\
                shft = shft>>w,\
                shft = SIGNXY(shft,x))

#define IQUANT_H264(x,y,wscal,w,shft) (shft = x, \
                shft *=y, \
                shft *=wscal, \
                shft = shft<<w)

#define IQUANT_lev_H264(x,y,wscal,add_f,w,shft) (shft = x, \
                shft *=y, \
                shft *=wscal, \
                shft+= add_f, \
                shft = shft>>w)

#endif /* IH264_TRANS_MACROS_H_ */
