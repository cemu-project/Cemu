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
 * Modified for use with Cemu emulator project
*/
/**
*******************************************************************************
* @file
*  iv.h
*
* @brief
*  This file contains all the necessary structure and  enumeration
* definitions needed for the Application  Program Interface(API) of the
* Ittiam Video and Image  codecs
*
* @author
*  100239(RCY)
*
* @par List of Functions:
*
* @remarks
*  None
*
*******************************************************************************
*/


#ifndef _IV_H
#define _IV_H

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/


/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/


/* IV_API_CALL_STATUS_T:This is only to return the FAIL/PASS status to the  */
/* application for the current API call                                     */

typedef enum {
    IV_STATUS_NA                                = 0x7FFFFFFF,
    IV_SUCCESS                                  = 0x0,
    IV_FAIL                                     = 0x1,
}IV_API_CALL_STATUS_T;


/* IV_COLOR_FORMAT_T: This enumeration lists all the color formats which    */
/* finds usage in video/image codecs                                        */

typedef enum {
    IV_CHROMA_NA                            = 0x7FFFFFFF,
    IV_YUV_420P                             = 0x1,
    IV_YUV_422P                             = 0x2,
    IV_420_UV_INTL                          = 0x3,
    IV_YUV_422IBE                           = 0x4,
    IV_YUV_422ILE                           = 0x5,
    IV_YUV_444P                             = 0x6,
    IV_YUV_411P                             = 0x7,
    IV_GRAY                                 = 0x8,
    IV_RGB_565                              = 0x9,
    IV_RGB_24                               = 0xa,
    IV_YUV_420SP_UV                         = 0xb,
    IV_YUV_420SP_VU                         = 0xc,
    IV_RGBA_8888                            = 0xd
}IV_COLOR_FORMAT_T;

/* IV_PICTURE_CODING_TYPE_T: VOP/Frame coding type Enumeration              */

typedef enum {
    IV_NA_FRAME                             = 0x7FFFFFFF,
    IV_I_FRAME                              = 0x0,
    IV_P_FRAME                              = 0x1,
    IV_B_FRAME                              = 0x2,
    IV_IDR_FRAME                            = 0x3,
    IV_II_FRAME                             = 0x4,
    IV_IP_FRAME                             = 0x5,
    IV_IB_FRAME                             = 0x6,
    IV_PI_FRAME                             = 0x7,
    IV_PP_FRAME                             = 0x8,
    IV_PB_FRAME                             = 0x9,
    IV_BI_FRAME                             = 0xa,
    IV_BP_FRAME                             = 0xb,
    IV_BB_FRAME                             = 0xc,
    IV_MBAFF_I_FRAME                        = 0xd,
    IV_MBAFF_P_FRAME                        = 0xe,
    IV_MBAFF_B_FRAME                        = 0xf,
    IV_MBAFF_IDR_FRAME                      = 0x10,
    IV_NOT_CODED_FRAME                      = 0x11,
    IV_FRAMETYPE_DEFAULT                    = IV_I_FRAME
}IV_PICTURE_CODING_TYPE_T;

/* IV_FLD_TYPE_T: field type Enumeration                                    */

typedef enum {
    IV_NA_FLD                               = 0x7FFFFFFF,
    IV_TOP_FLD                              = 0x0,
    IV_BOT_FLD                              = 0x1,
    IV_FLD_TYPE_DEFAULT                     = IV_TOP_FLD
}IV_FLD_TYPE_T;

/* IV_CONTENT_TYPE_T: Video content type                                     */

typedef enum {
    IV_CONTENTTYPE_NA                       = 0x7FFFFFFF,
    IV_PROGRESSIVE                          = 0x0,
    IV_INTERLACED                           = 0x1,
    IV_PROGRESSIVE_FRAME                    = 0x2,
    IV_INTERLACED_FRAME                     = 0x3,
    IV_INTERLACED_TOPFIELD                  = 0x4,
    IV_INTERLACED_BOTTOMFIELD               = 0x5,
    IV_CONTENTTYPE_DEFAULT                  = IV_PROGRESSIVE,
}IV_CONTENT_TYPE_T;

/* IV_API_COMMAND_TYPE_T:API command type                                   */
typedef enum {
    IV_CMD_NA                           = 0x7FFFFFFF,
    IV_CMD_DUMMY_ELEMENT                = 0x4,
}IV_API_COMMAND_TYPE_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/* IV_OBJ_T: This structure defines the handle for the codec instance        */

typedef struct {
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     * Pointer to the API function pointer table of the codec
     */
    void                                        *pv_fxns;

    /**
     * Pointer to the handle of the codec
     */
    void                                        *pv_codec_handle;
}iv_obj_t;


/* IV_YUV_BUF_T: This structure defines attributes for the yuv buffer        */

typedef struct {
    /**
     * u4_size of the structure
     */
    UWORD32                                     u4_size;

    /**
     * Pointer to Luma (Y) Buffer
     */

    void                                        *pv_y_buf;
    /**
     * Pointer to Chroma (Cb) Buffer
     */
    void                                        *pv_u_buf;

    /**
     * Pointer to Chroma (Cr) Buffer
     */
    void                                        *pv_v_buf;

    /**
     * Width of the Luma (Y) Buffer
     */
    UWORD32                                     u4_y_wd;

    /**
     * Height of the Luma (Y) Buffer
     */
    UWORD32                                     u4_y_ht;

    /**
     * Stride/Pitch of the Luma (Y) Buffer
     */
    UWORD32                                     u4_y_strd;

    /**
     * Width of the Chroma (Cb) Buffer
     */
    UWORD32                                     u4_u_wd;

    /**
     * Height of the Chroma (Cb) Buffer
     */
    UWORD32                                     u4_u_ht;

    /**
     * Stride/Pitch of the Chroma (Cb) Buffer
     */
    UWORD32                                     u4_u_strd;

    /**
     * Width of the Chroma (Cr) Buffer
     */
    UWORD32                                     u4_v_wd;

    /**
     * Height of the Chroma (Cr) Buffer
     */
    UWORD32                                     u4_v_ht;

    /**
     * Stride/Pitch of the Chroma (Cr) Buffer
     */
    UWORD32                                     u4_v_strd;
}iv_yuv_buf_t;



#endif /* _IV_H */

