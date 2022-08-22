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
*  ih264_defs.h
*
* @brief
*  Definitions used in the codec
*
* @author
*  Ittiam
*
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef IH264_DEFS_H_
#define IH264_DEFS_H_

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/


/*****************************************************************************/
/* Profile and Levels                                                        */
/*****************************************************************************/

/**
******************************************************************************
 *  @enum  PROFILE_IDC
 *  @brief Defines the set of possible profiles
******************************************************************************
*/
enum
{
    IH264_PROFILE_BASELINE = 66,
    IH264_PROFILE_MAIN = 77,
    IH264_PROFILE_EXTENDED = 88,
    IH264_PROFILE_HIGH = 100,
    IH264_PROFILE_HIGH10 = 110,
    IH264_PROFILE_HIGH422 = 122,
    IH264_PROFILE_HIGH444 = 144,
};

/**
******************************************************************************
 *  @enum  LEVEL_IDC
 *  @brief Defines the set of possible levels
******************************************************************************
*/
typedef enum
{
    IH264_LEVEL_10         = 10,
    IH264_LEVEL_1B         = 9,
    IH264_LEVEL_11         = 11,
    IH264_LEVEL_12         = 12,
    IH264_LEVEL_13         = 13,
    IH264_LEVEL_20         = 20,
    IH264_LEVEL_21         = 21,
    IH264_LEVEL_22         = 22,
    IH264_LEVEL_30         = 30,
    IH264_LEVEL_31         = 31,
    IH264_LEVEL_32         = 32,
    IH264_LEVEL_40         = 40,
    IH264_LEVEL_41         = 41,
    IH264_LEVEL_42         = 42,
    IH264_LEVEL_50         = 50,
    IH264_LEVEL_51         = 51,
}IH264_LEVEL_T;


/**
******************************************************************************
 *  @enum  PIC TYPES
 *  @brief Defines the set of possible picture type - not signaled in bitstream
******************************************************************************
*/
typedef enum
{
    PIC_NA = 0x7FFFFFFF,
    PIC_IDR = 0,
    PIC_I = 1,
    PIC_P = 2,
    PIC_B = 3,
    PIC_P_NONREF = 4,
    PIC_B_NONREF = 5,
    PIC_MAX,
}PIC_TYPE_T;

/**
******************************************************************************
 *  @enum  FRAME-FIELD types
 *  @brief Defines the set of possible field types.
******************************************************************************
*/
enum
{
    TOP_FIELD,
    BOTTOM_FIELD,
    FRAME,
};

/**
******************************************************************************
 *  @enum  SLICE TYPES
 *  @brief Defines the set of possible SLICE TYPES
******************************************************************************
*/
enum
{
    PSLICE = 0,
    BSLICE = 1,
    ISLICE = 2,
    SPSLICE = 3,
    SISLICE = 4,
    MAXSLICE_TYPE,
};

/**
******************************************************************************
 *  @enum  NAL_UNIT_TYPE
 *  @brief Defines the set of possible nal unit types
******************************************************************************
*/
enum
{
    NAL_UNSPEC_0        = 0,
    NAL_SLICE_NON_IDR   = 1,
    NAL_SLICE_DPA       = 2,
    NAL_SLICE_DPB       = 3,
    NAL_SLICE_DPC       = 4,
    NAL_SLICE_IDR       = 5,
    NAL_SEI             = 6,
    NAL_SPS             = 7,
    NAL_PPS             = 8,
    NAL_AUD             = 9,
    NAL_EOSEQ           = 10,
    NAL_EOSTR           = 11,
    NAL_FILLER          = 12,
    NAL_SPSE            = 13,
    NAL_RES_18          = 14,
    NAL_AUX_PIC         = 19,
    NAL_RES_23          = 20,
    NAL_UNSPEC_31       = 24,
};

/**
******************************************************************************
 *  @enum  CHROMA_FORMAT_IDC
 *  @brief Defines the set of possible chroma formats
 *  Note Chorma format Do not change enum values
******************************************************************************
*/
enum
{
    CHROMA_FMT_IDC_MONOCHROME   = 0,
    CHROMA_FMT_IDC_YUV420       = 1,
    CHROMA_FMT_IDC_YUV422       = 2,
    CHROMA_FMT_IDC_YUV444       = 3,
    CHROMA_FMT_IDC_YUV444_PLANES = 4,
};


/**
******************************************************************************
 *  @enum  MBMODES_I16x16
 *  @brief Defines the set of possible intra 16x16 mb modes
******************************************************************************
*/
typedef enum
{
    VERT_I16x16     = 0,
    HORZ_I16x16     = 1,
    DC_I16x16       = 2,
    PLANE_I16x16    = 3,
    MAX_I16x16      = 4,
}MBMODES_I16x16;

/**
******************************************************************************
 *  @enum  MBMODES_I4x4
 *  @brief Defines the set of possible intra 4x4 mb modes
******************************************************************************
*/
typedef enum
{
    VERT_I4x4     = 0,
    HORZ_I4x4     = 1,
    DC_I4x4       = 2,
    DIAG_DL_I4x4  = 3,
    DIAG_DR_I4x4  = 4,
    VERT_R_I4x4   = 5,
    HORZ_D_I4x4   = 6,
    VERT_L_I4x4   = 7,
    HORZ_U_I4x4   = 8,
    MAX_I4x4      = 9,
}MBMODES_I4x4;

/**
******************************************************************************
 *  @enum  MBMODES_I8x8
 *  @brief Defines the set of possible intra 8x8 mb modes
******************************************************************************
*/
typedef enum
{
    VERT_I8x8     = 0,
    HORZ_I8x8     = 1,
    DC_I8x8       = 2,
    DIAG_DL_I8x8  = 3,
    DIAG_DR_I8x8  = 4,
    VERT_R_I8x8   = 5,
    HORZ_D_I8x8   = 6,
    VERT_L_I8x8   = 7,
    HORZ_U_I8x8   = 8,
    MAX_I8x8      = 9,
}MBMODES_I8x8;

/**
******************************************************************************
 *  @enum  MBMODES_CHROMA_I8x8 (Chroma)
 *  @brief Defines the set of possible intra 8x8 mb modes for chroma
******************************************************************************
*/
typedef enum
{
    DC_CH_I8x8     = 0,
    HORZ_CH_I8x8   = 1,
    VERT_CH_I8x8   = 2,
    PLANE_CH_I8x8  = 3,
    MAX_CH_I8x8    = 4,
}MBMODES_CHROMA_I8x8;

/**
******************************************************************************
 *  @enum  MBTYPES
 *  @brief Defines the set of possible macro block types
******************************************************************************
*/
typedef enum
{
    I16x16      = 0,
    I4x4        = 1,
    I8x8        = 2,
    P16x16      = 3,
    P16x8       = 4,
    P8x16       = 5,
    P8x8        = 6,
    PSKIP       = 7,
    IPCM        = 8,
    B16x16      = 9,
    BSKIP       = 10,
    BDIRECT     = 11,
    MAX_MBTYPES,
}MBTYPES_T;

/* Prediction list */
/* Do not change enum values */
enum
{
    PRED_L0 = 0,
    PRED_L1 = 1,
    PRED_BI = 2
};


/**
******************************************************************************
 *  @enum  ENTROPY_BLK_TYPE
 *  @brief Defines the nature of blocks employed in entropy coding
******************************************************************************
*/
typedef enum
{
    ENTROPY_BLK_INVALID = -1,
    CAVLC_LUMA_4x4_DC = 0,
    CAVLC_LUMA_4x4_AC = 1,
    CAVLC_LUMA_4x4 = 2,
    CAVLC_CHROMA_4x4_DC = 3,
    CAVLC_CHROMA_4x4_AC = 4,
} ENTROPY_BLK_TYPE;

/**
******************************************************************************
 *  @enum  ENTROPY_MODE
 *  @brief Entropy coding modes
******************************************************************************
*/
typedef enum
{
    CAVLC = 0,
    CABAC = 1,
} ENTROPY_MODE;

/**
******************************************************************************
 *  @enum  COMPONENT_TYPE
 *  @brief components Y, U & V
******************************************************************************
*/
typedef enum
{
    Y,
    U,
    V,
} COMPONENT_TYPE;


/**
******************************************************************************
 *  @enum  MBPART_PREDMODE_T
 *  @brief MbPartps_pred_mode_ctxt Table 7-11 to 7-14
******************************************************************************
*/
typedef enum
{
    MBPART_NA,
    MBPART_I4x4,
    MBPART_I8x8,
    MBPART_I16x16,
    MBPART_L0,
    MBPART_L1,
    MBPART_BI,
    MBPART_DIRECT,
    MBPART_IPCM,
}MBPART_PREDMODE_T;


typedef enum
{
    I_NxN,
    I_16x16_0_0_0,
    I_16x16_1_0_0,
    I_16x16_2_0_0,
    I_16x16_3_0_0,
    I_16x16_0_1_0,
    I_16x16_1_1_0,
    I_16x16_2_1_0,
    I_16x16_3_1_0,
    I_16x16_0_2_0,
    I_16x16_1_2_0,
    I_16x16_2_2_0,
    I_16x16_3_2_0,
    I_16x16_0_0_1,
    I_16x16_1_0_1,
    I_16x16_2_0_1,
    I_16x16_3_0_1,
    I_16x16_0_1_1,
    I_16x16_1_1_1,
    I_16x16_2_1_1,
    I_16x16_3_1_1,
    I_16x16_0_2_1,
    I_16x16_1_2_1,
    I_16x16_2_2_1,
    I_16x16_3_2_1,
    I_PCM,
}MBTYPE_ISLICE_T;

typedef enum
{
    P_L0_16x16,
    P_L0_L0_16x8,
    P_L0_L0_8x16,
    P_8x8,
    P_8x8REF0,
    P_SKIP
}MBTYPE_PSLICE_T;

typedef enum
{
    B_DIRECT_16x16,
    B_L0_16x16,
    B_L1_16x16,
    B_BI_16x16,
    B_L0_L0_16x8,
    B_L0_L0_8x16,
    B_L1_L1_16x8,
    B_L1_L1_8x16,
    B_L0_L1_16x8,
    B_L0_L1_8x16,
    B_L1_L0_16x8,
    B_L1_L0_8x16,
    B_L0_BI_16x8,
    B_L0_BI_8x16,
    B_L1_BI_16x8,
    B_L1_BI_8x16,
    B_BI_L0_16x8,
    B_BI_L0_8x16,
    B_BI_L1_16x8,
    B_BI_L1_8x16,
    B_BI_BI_16x8,
    B_BI_BI_8x16,
    B_8x8,
    B_SKIP,
}MBTYPE_BSLICE_T;


typedef enum
{
    P_L0_8x8,
    P_L0_8x4,
    P_L0_4x8,
    P_L0_4x4,
}SUBMBTYPE_PSLICE_T;

typedef enum
{
    B_DIRECT_8x8,
    B_L0_8x8,
    B_L1_8x8,
    B_BI_8x8,
    B_L0_8x4,
    B_L0_4x8,
    B_L1_8x4,
    B_L1_4x8,
    B_BI_8x4,
    B_BI_4x8,
    B_L0_4x4,
    B_L1_4x4,
    B_BI_4x4,
}SUBMBTYPE_BSLICE_T;

/**
 * DC Mode pattern for 4 4x4 sub blocks in an MB row
 */
#define DC_I16X16_MB_ROW (DC_I16x16 << 24) | (DC_I16x16 << 16) | \
                         (DC_I16x16 << 8)  | DC_I16x16



/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Reference frame defs                                                      */
/*****************************************************************************/
/* Maximum DPB size */
#define MAX_DPB_SIZE 16

/* Maximum mmco commands in slice header */
#define MAX_MMCO_COMMANDS 32

/* Maximum reference reorder idc */
#define MAX_MODICATION_IDC 32

/*****************************************************************************/
/* SPS restrictions                                                          */
/*****************************************************************************/

/* Number of SPS allowed */
/* An extra buffer is allocated to write the parsed data
 * It is copied to the appropriate location later */
#define MAX_SPS_CNT         (32 + 1)

/* Maximum long term reference pics */
#define MAX_LTREF_PICS_SPS 16

/* Maximum short term reference pics */
#define MAX_STREF_PICS_SPS 64


/*****************************************************************************/
/* PPS restrictions                                                          */
/*****************************************************************************/

/* Number of PPS allowed  */
/* An extra buffer is allocated to write the parsed data
 * It is copied to the appropriate location later */
#define MAX_PPS_CNT         (256 + 1)

/*****************************************************************************/
/* Macro definitions for sizes of MB, PU, TU, CU                            */
/*****************************************************************************/
#define MB_SIZE             16
#define BLK8x8SIZE          8
#define BLK_SIZE            4


/* TU Size Range */
#define MAX_TU_SIZE         8
#define MIN_TU_SIZE         4

/* Max Transform Size */
#define MAX_TRANS_SIZE      (MAX_TU_SIZE*MAX_TU_SIZE)

/* PU Size Range */
#define MAX_PU_SIZE         16
#define MIN_PU_SIZE         4

/* Number of max TU in a MB row */
#define MAX_TU_IN_MB_ROW   ((MB_SIZE / MIN_TU_SIZE))

/* Number of max PU in a CTb row */
#define MAX_PU_IN_MB_ROW   ((MB_SIZE / MIN_PU_SIZE))


/* Number of max PU in a MB */
/*****************************************************************************/
/* Note though for 64 x 64 MB, Max PU in MB is 128, in order to store      */
/*  intra pred info, 256 entries are needed                                  */
/*****************************************************************************/
#define MAX_PU_IN_MB       ((MB_SIZE / MIN_PU_SIZE) * \
                             (MB_SIZE / MIN_PU_SIZE))

/* Number of max TU in a MB */
#define MAX_TU_IN_MB       ((MB_SIZE / MIN_TU_SIZE) * \
                             (MB_SIZE / MIN_TU_SIZE))



/**
 * Maximum transform depths
 */
#define MAX_TRAFO_DEPTH 5

#define MAX_DC_4x4_SUBBLK_LUMA 1
#define MAX_AC_4x4_SUBBLK_LUMA 16
#define MAX_DC_4x4_SUBBLK_CHROMA 2
#define MAX_AC_4x4_SUBBLK_CHROMA 8

#define MAX_4x4_SUBBLKS (MAX_DC_4x4_SUBBLK_LUMA + MAX_DC_4x4_SUBBLK_CHROMA +\
                         MAX_AC_4x4_SUBBLK_LUMA + MAX_AC_4x4_SUBBLK_CHROMA)

/* Max number of deblocking edges */
#define MAX_VERT_DEBLK_EDGES ((MB_SIZE/8) * (MB_SIZE/4))
#define MAX_HORZ_DEBLK_EDGES ((MB_SIZE/4) * (MB_SIZE/8))

/* Qp can not change below 8x8 level */
#define MAX_DEBLK_QP_CNT     ((MB_SIZE/8) * (MB_SIZE/8))

/*****************************************************************************/
/* Parsing related macros                                                    */
/*****************************************************************************/
#define SUBBLK_COEFF_CNT    16

/* Quant and Trans defs */

/*****************************************************************************/
/* Sizes for Transform functions                                             */
/*****************************************************************************/
#define TRANS_SIZE_4   4
#define TRANS_SIZE_8   8
#define TRANS_SIZE_16 16
#define TRANS_SIZE_32 32


#define IT_SHIFT_STAGE_1 7
#define IT_SHIFT_STAGE_2 12

/**
 * @breif  Maximum transform dynamic range (excluding sign bit)
 */
#define MAX_TR_DYNAMIC_RANGE  15

/**
 * @brief  Q(QP%6) * IQ(QP%6) = 2^20
 */
#define QUANT_IQUANT_SHIFT    20

/**
 * @breif Q factor for Qp%6 multiplication
 */
#define QUANT_SHIFT           14

/**
 * @breif Q shift factor for flat rescale matrix weights
 */
#define FLAT_RESCALE_MAT_Q_SHIFT    11

/**
 * @breif  Scaling matrix is represented in Q15 format
 */
#define SCALING_Q_SHIFT       15

/**
 * @brief  rounding factor for quantization represented in Q9 format
 */
#define QUANT_ROUND_FACTOR_Q   9

/**
 * @brief  Minimum qp supported in H264 spec
 */
#define MIN_H264_QP 0

/**
 * @brief  Maximum qp supported in H264 spec
 */
#define MAX_H264_QP 51

/**
 * @brief  Minimum delta scale supported in H264 spec
 */
#define MIN_H264_DELTA_SCALE (-128)

/**
 * @brief  Maximum delta scale supported in H264 spec
 */
#define MAX_H264_DELTA_SCALE 127

/**
 * @breif  Total number of transform sizes
 * used for sizeID while getting scale matrix
 */
#define NUM_UNIQUE_TRANS_SIZE 4

/**
 * @breif  Maximum number of bits in frameNumber signaling
 */
#define MAX_BITS_IN_FRAME_NUM     16

/**
 * @breif  Maximum number of bits in POC LSB signaling
 */
#define MAX_BITS_IN_POC_LSB     16


/**
 * @breif  Maximum PIC Order Count type
 */
#define MAX_PIC_ORDER_COUNT_TYPE    2


/**
 * @breif  Maximum Weighted bipred idc
 */
#define MAX_WEIGHT_BIPRED_IDC 2

/*****************************************************************************/
/* Number of scaling matrices for each transform size                        */
/*****************************************************************************/
#define SCALE_MAT_CNT_TRANS_SIZE_4    6
#define SCALE_MAT_CNT_TRANS_SIZE_8    6
#define SCALE_MAT_CNT_TRANS_SIZE_16   6
#define SCALE_MAT_CNT_TRANS_SIZE_32   2

/* Maximum number of scale matrices for a given transform size */
#define SCALE_MAT_CNT_MAX_PER_TRANS_SIZE 6

/* Total number of scale matrices */
#define TOTAL_SCALE_MAT_COUNT   (SCALE_MAT_CNT_TRANS_SIZE_4     + \
                                 SCALE_MAT_CNT_TRANS_SIZE_8     + \
                                 SCALE_MAT_CNT_TRANS_SIZE_16    + \
                                 SCALE_MAT_CNT_TRANS_SIZE_32)


/*****************************************************************************/
/* Intra pred Macros                                                         */
/*****************************************************************************/
/** Planar Intra prediction mode */
#define INTRA_PLANAR             0

/** DC Intra prediction mode */
#define INTRA_DC                 1

/** Gives angular mode for intra prediction */
#define INTRA_ANGULAR(x) (x)

/** Following is used to signal no intra prediction in case of pcm blocks
 */
#define INTRA_PRED_NONE  63


/** Following is used to signal no intra prediction is needed for first three
 * 4x4 luma blocks in case of 4x4 TU sizes
 * Also used in pcm cases
 */
#define INTRA_PRED_CHROMA_IDX_NONE  7


/**
******************************************************************************
 *  @brief  neighbor availability masks
******************************************************************************
 */
#define LEFT_MB_AVAILABLE_MASK      0x01
#define TOP_LEFT_MB_AVAILABLE_MASK  0x02
#define TOP_MB_AVAILABLE_MASK       0x04
#define TOP_RIGHT_MB_AVAILABLE_MASK 0x08

/**
******************************************************************************
 *  @brief  SEI macros
******************************************************************************
 */
/*
 * @brief  specifies the number of colour primary components of the mastering display
 */
#define NUM_SEI_MDCV_PRIMARIES      3

/*
 * @brief  specifies the number of colour primary components of the nominal content colour volume
 */
#define NUM_SEI_CCV_PRIMARIES       3

#define DISPLAY_PRIMARIES_X_UPPER_LIMIT                37000
#define DISPLAY_PRIMARIES_X_LOWER_LIMIT                5
#define DISPLAY_PRIMARIES_X_DIVISION_FACTOR            5

#define DISPLAY_PRIMARIES_Y_UPPER_LIMIT                42000
#define DISPLAY_PRIMARIES_Y_LOWER_LIMIT                5
#define DISPLAY_PRIMARIES_Y_DIVISION_FACTOR            5

#define WHITE_POINT_X_UPPER_LIMIT                      37000
#define WHITE_POINT_X_LOWER_LIMIT                      5
#define WHITE_POINT_X_DIVISION_FACTOR                  5

#define WHITE_POINT_Y_UPPER_LIMIT                      42000
#define WHITE_POINT_Y_LOWER_LIMIT                      5
#define WHITE_POINT_Y_DIVISION_FACTOR                  5

#define MAX_DISPLAY_MASTERING_LUMINANCE_UPPER_LIMIT        100000000
#define MAX_DISPLAY_MASTERING_LUMINANCE_LOWER_LIMIT        50000
#define MAX_DISPLAY_MASTERING_LUMINANCE_DIVISION_FACTOR    10000

#define MIN_DISPLAY_MASTERING_LUMINANCE_UPPER_LIMIT        50000
#define MIN_DISPLAY_MASTERING_LUMINANCE_LOWER_LIMIT        1

#define AMBIENT_LIGHT_X_UPPER_LIMIT        50000
#define AMBIENT_LIGHT_Y_UPPER_LIMIT        50000

#define CCV_PRIMARIES_X_UPPER_LIMIT        5000000
#define CCV_PRIMARIES_X_LOWER_LIMIT        -5000000
#define CCV_PRIMARIES_Y_UPPER_LIMIT        5000000
#define CCV_PRIMARIES_Y_LOWER_LIMIT        -5000000

#endif /* IH264_DEFS_H_ */
