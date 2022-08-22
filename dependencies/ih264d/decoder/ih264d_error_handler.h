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

#ifndef _IH264D_ERROR_HANDLER_H_
#define _IH264D_ERROR_HANDLER_H_

/*!
 *************************************************************************
 * \file ih264d_error_handler.h
 *
 * \brief
 *    Contains declaration of ih264d_global_error_handler function
 *
 * \date
 *    21/11/2002
 *
 * \author  AI
 *************************************************************************
 */

#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_structs.h"

typedef enum
{

    ERROR_MEM_ALLOC_ISRAM_T = 0x50,
    ERROR_MEM_ALLOC_SDRAM_T = 0x51,
    ERROR_BUF_MGR = 0x52,
    ERROR_DBP_MANAGER_T = 0x53,
    ERROR_GAPS_IN_FRM_NUM = 0x54,
    ERROR_UNKNOWN_NAL = 0x55,
    ERROR_INV_MB_SLC_GRP_T = 0x56,
    ERROR_MULTIPLE_SLC_GRP_T = 0x57,
    ERROR_UNKNOWN_LEVEL = 0x58,
    ERROR_FEATURE_UNAVAIL = 0x59,
    ERROR_NOT_SUPP_RESOLUTION = 0x5A,
    ERROR_INVALID_PIC_PARAM = 0x5B,
    ERROR_INVALID_SEQ_PARAM = 0x5C,
    ERROR_EGC_EXCEED_32_1_T = 0x5D,
    ERROR_EGC_EXCEED_32_2_T = 0x5E,
    ERROR_INV_RANGE_TEV_T = 0x5F,
    ERROR_INV_SLC_TYPE_T = 0x60,
    ERROR_UNAVAIL_PICBUF_T = 0x61,
    ERROR_UNAVAIL_MVBUF_T = 0x62,
    ERROR_UNAVAIL_DISPBUF_T = 0x63,
    ERROR_INV_POC_TYPE_T = 0x64,
    ERROR_PIC1_NOT_FOUND_T = 0x65,
    ERROR_PIC0_NOT_FOUND_T = 0x66,
    ERROR_NUM_REF = 0x67,
    ERROR_REFIDX_ORDER_T = 0x68,
    ERROR_EOB_FLUSHBITS_T = 0x69,
    ERROR_EOB_GETBITS_T = 0x6A,
    ERROR_EOB_GETBIT_T = 0x6B,
    ERROR_EOB_BYPASS_T = 0x6C,
    ERROR_EOB_DECISION_T = 0x6D,
    ERROR_EOB_TERMINATE_T = 0x6E,
    ERROR_EOB_READCOEFF4X4CAB_T = 0x6F,
    ERROR_INV_RANGE_QP_T = 0x70,
    ERROR_END_OF_FRAME_EXPECTED_T = 0x71,
    ERROR_MB_TYPE = 0x72,
    ERROR_SUB_MB_TYPE = 0x73,
    ERROR_CBP = 0x74,
    ERROR_REF_IDX = 0x75,
    ERROR_NUM_MV = 0x76,
    ERROR_CHROMA_PRED_MODE = 0x77,
    ERROR_INTRAPRED = 0x78,
    ERROR_NEXT_MB_ADDRESS_T = 0x79,
    ERROR_MB_ADDRESS_T = 0x7A,
    ERROR_MB_GROUP_ASSGN_T = 0x7B,
    ERROR_CAVLC_NUM_COEFF_T = 0x7C,
    ERROR_CAVLC_SCAN_POS_T = 0x7D,
    ERROR_CABAC_RENORM_T = 0x7E,
    ERROR_CABAC_SIG_COEFF1_T = 0x7F,
    ERROR_CABAC_SIG_COEFF2_T = 0x80,
    ERROR_CABAC_ENCODE_COEFF_T = 0x81,
    ERROR_INV_SPS_PPS_T = 0x82,
    ERROR_INV_SLICE_HDR_T = 0x83,
    ERROR_PRED_WEIGHT_TABLE_T = 0x84,
    IH264D_VERS_BUF_INSUFFICIENT = 0x85,
    ERROR_ACTUAL_LEVEL_GREATER_THAN_INIT = 0x86,
    ERROR_CORRUPTED_SLICE = 0x87,
    ERROR_FRAME_LIMIT_OVER = 0x88,
    ERROR_ACTUAL_RESOLUTION_GREATER_THAN_INIT = 0x89,
    ERROR_PROFILE_NOT_SUPPORTED = 0x8A,
    ERROR_DISP_WIDTH_RESET_TO_PIC_WIDTH = 0x8B,
    ERROR_DISP_WIDTH_INVALID = 0x8C,
    ERROR_DANGLING_FIELD_IN_PIC = 0x8D,
    ERROR_DYNAMIC_RESOLUTION_NOT_SUPPORTED = 0x8E,
    ERROR_INIT_NOT_DONE = 0x8F,
    ERROR_LEVEL_UNSUPPORTED = 0x90,
    ERROR_START_CODE_NOT_FOUND = 0x91,
    ERROR_PIC_NUM_IS_REPEATED = 0x92,
    ERROR_IN_LAST_SLICE_OF_PIC = 0x93,
    ERROR_NEW_FRAME_EXPECTED = 0x94,
    ERROR_INCOMPLETE_FRAME = 0x95,
    ERROR_VUI_PARAMS_NOT_FOUND = 0x96,
    ERROR_INV_POC = 0x97,
    ERROR_SEI_MDCV_PARAMS_NOT_FOUND = 0x98,
    ERROR_SEI_CLL_PARAMS_NOT_FOUND = 0x99,
    ERROR_SEI_AVE_PARAMS_NOT_FOUND = 0x9A,
    ERROR_SEI_CCV_PARAMS_NOT_FOUND = 0x9B,
    ERROR_INV_SEI_MDCV_PARAMS = 0x9C,
    ERROR_INV_SEI_CLL_PARAMS = 0x9D,
    ERROR_INV_SEI_AVE_PARAMS = 0x9E,
    ERROR_INV_SEI_CCV_PARAMS = 0x9F,
    ERROR_INV_FRAME_NUM = 0xA0

} h264_decoder_error_code_t;

WORD32 ih264d_mark_err_slice_skip(dec_struct_t * ps_dec,
                                  WORD32 num_mb_skip,
                                  UWORD8 u1_is_idr_slice,
                                  UWORD16 u2_frame_num,
                                  pocstruct_t *ps_cur_poc,
                                  WORD32 prev_slice_err);

void ih264d_err_pic_dispbuf_mgr(dec_struct_t *ps_dec);
#endif /* _IH264D_ERROR_HANDLER_H_ */
