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
#ifndef _IH264D_DEFS_H_
#define _IH264D_DEFS_H_

/**
 ************************************************************************
 * \file ih264d_defs.h
 *
 * \brief
 *    Type definitions used in the code
 *
 * \date
 *    19/11/2002
 *
 * \author  Sriram Sethuraman
 *
 ************************************************************************
 */
#include <stdint.h>

#define H264_MAX_FRAME_WIDTH                4080
#define H264_MAX_FRAME_HEIGHT               4080
#define H264_MAX_FRAME_SIZE                 (4096 * 2048)

#define H264_MIN_FRAME_WIDTH                16
#define H264_MIN_FRAME_HEIGHT               16

#define FMT_CONV_NUM_ROWS       16

/** Decoder currently has an additional latency of 2 pictures when
  * returning output for display
  */
#define DISPLAY_LATENCY         2

/** Bit manipulation macros */
#define CHECKBIT(a,i) ((a) &  (1 << i))
#define CLEARBIT(a,i) ((a) &= ~(1 << i))

/** Macro to check if a number lies in the valid integer range */
#define IS_OUT_OF_RANGE_S32(a) (((a) < INT32_MIN) || ((a) > INT32_MAX))

/** Macro to convert a integer to a boolean value */
#define BOOLEAN(x) (!!(x))

/** Arithmetic operations */
#define MOD(x,y) ((x)%(y))
#define DIV(x,y) ((x)/(y))
#define MUL(x,y) ((x)*(y))
#define SIGN_POW2_DIV(x, y) (((x) < 0) ? (-((-(x)) >> (y))) : ((x) >> (y)))

#define MB_ENABLE_FILTERING           0x00
#define MB_DISABLE_FILTERING          0x01
#define MB_DISABLE_TOP_EDGE           0x02
#define MB_DISABLE_LEFT_EDGE          0x04

/** Maximum number of reference pics */
#define MAX_REF_BUFS    32
#define MAX_DISP_BUFS_NEW 64
#define MAX_FRAMES              16

#define INVALID_FRAME_NUM       0x0fffffff
#define GAP_FRAME_NUM           0x1fffffff

/** macros for reference picture lists, refIdx to POC mapping */
// 1 extra entry into reference picture lists for refIdx = -1.
// this entry is always 0. this saves conditional checks in
// FillBs modules.
#define POC_LIST_L0_TO_L1_DIFF  (( 2*MAX_FRAMES) + 1)
#define POC_LIST_L0_TO_L1_DIFF_1  ((MAX_FRAMES) + 1)

#define FRM_LIST_L0             0                                               //0
#define FRM_LIST_L1             1 * POC_LIST_L0_TO_L1_DIFF//FRM_LIST_L0 + POC_LIST_L0_TO_L1_DIFF        //0+33                  //(1 * POC_LIST_L0_TO_L1_DIFF)
#define TOP_LIST_FLD_L0         2 * POC_LIST_L0_TO_L1_DIFF//FRM_LIST_L1 + POC_LIST_L0_TO_L1_DIFF        //0+33+33                   //(2 * POC_LIST_L0_TO_L1_DIFF)
#define TOP_LIST_FLD_L1         3 * POC_LIST_L0_TO_L1_DIFF//TOP_LIST_FLD_L0 + POC_LIST_L0_TO_L1_DIFF_1  //0+33+33+17                //(3 * POC_LIST_L0_TO_L1_DIFF)
#define BOT_LIST_FLD_L0         4 * POC_LIST_L0_TO_L1_DIFF//TOP_LIST_FLD_L1 + POC_LIST_L0_TO_L1_DIFF_1  //0+33+33+17+17
#define BOT_LIST_FLD_L1         5 * POC_LIST_L0_TO_L1_DIFF//BOT_LIST_FLD_L0 + POC_LIST_L0_TO_L1_DIFF_1  //0+33+33+17+17+17
#define TOTAL_LIST_ENTRIES      6 * POC_LIST_L0_TO_L1_DIFF//BOT_LIST_FLD_L1 + POC_LIST_L0_TO_L1_DIFF_1  //0+33+33+17+17+17+17
#define PAD_MV_BANK_ROW             64
#define OFFSET_MV_BANK_ROW          ((PAD_MV_BANK_ROW)>>1)
#define PAD_PUC_CURNNZ              32
#define OFFSET_PUC_CURNNZ           (PAD_PUC_CURNNZ)
#define PAD_MAP_IDX_POC             (1)
#define OFFSET_MAP_IDX_POC          (1)

#define OFFSET_MAP_IDX_POC          (1)

#define NAL_REF_IDC(nal_first_byte)       ((nal_first_byte >> 5) & 0x3)
#define NAL_FORBIDDEN_BIT(nal_first_byte) (nal_first_byte>>7)
#define NAL_UNIT_TYPE(nal_first_byte)     (nal_first_byte & 0x1F)

#define INT_PIC_TYPE_I        (0x00)

#define YIELD_CNT_THRESHOLD  8


#define OK        0
#define END       1
#define NOT_OK    -1

/* For 420SP */
#define YUV420SP_FACTOR 2

/*To prevent buffer overflow access; in case the size of nal unit is
 *  greater than the allocated buffer size*/
#define EXTRA_BS_OFFSET 16*16*2

/**
 ***************************************************************************
 * Enum to hold various mem records being request
 ****************************************************************************
 */
enum
{
    /**
     * Codec Object at API level
     */
    MEM_REC_IV_OBJ,

    /**
     * Codec context
     */
    MEM_REC_CODEC,

    /**
     * Bitstream buffer which holds emulation prevention removed bytes
     */
    MEM_REC_BITSBUF,

    /**
     * Buffer to hold  coeff data
     */
    MEM_REC_COEFF_DATA,

    /**
     * Motion vector bank
     */
    MEM_REC_MVBANK,

    /**
     * Holds mem records passed to the codec.
     */
    MEM_REC_BACKUP,

    /**
     * Holds SPS
     */
    MEM_REC_SPS,

    /**
     * Holds PPS
     */
    MEM_REC_PPS,

    /**
     * Holds Slice Headers
     */
    MEM_REC_SLICE_HDR,

    /**
     * Holds thread handles
     */
    MEM_REC_THREAD_HANDLE,

    /**
     * Contains i4_status map indicating parse i4_status per MB basis
     */
    MEM_REC_PARSE_MAP,

    /**
     * Contains i4_status map indicating processing i4_status per MB basis
     */
    MEM_REC_PROC_MAP,

    /**
     * Contains slice number info for each MB
     */

    MEM_REC_SLICE_NUM_MAP,

    /**
     * Holds dpb manager context
     */
    MEM_REC_DPB_MGR,

    /**
     * Holds neighbors' info
     */
    MEM_REC_NEIGHBOR_INFO,

    /**
     * Holds neighbors' info
     */
    MEM_REC_PRED_INFO,


    /**
     * Holds inter pred inforamation on packed format info
     */
    MEM_REC_PRED_INFO_PKD,
    /**
     * Holds neighbors' info
     */
    MEM_REC_MB_INFO,

    /**
     * Holds deblock Mb info structure frame level)
     */
    MEM_REC_DEBLK_MB_INFO,

    /**
     * Holds  reference picture buffers in non-shared mode
     */
    MEM_REC_REF_PIC,

    /**
     * Holds  some misc intermediate_buffers
     */
    MEM_REC_EXTRA_MEM,

    /**
     * Holds  some misc intermediate_buffers
     */
    MEM_REC_INTERNAL_SCRATCH,

    /**
     * Holds  some misc intermediate_buffers
     */
    MEM_REC_INTERNAL_PERSIST,

    /* holds structures related to picture buffer manager*/
    MEM_REC_PIC_BUF_MGR,

    /*holds structure related to MV buffer manager*/
    MEM_REC_MV_BUF_MGR,

    /**
     * Place holder to compute number of memory records.
     */
    MEM_REC_CNT
/* Do not add anything below */
};

#ifdef DEBLOCK_THREAD
#define H264_MUTEX_LOCK(lock) ithread_mutex_lock(lock)
#define H264_MUTEX_UNLOCK(lock) ithread_mutex_unlock(lock)
#else //DEBLOCK_THREAD
#define H264_MUTEX_LOCK(lock)
#define H264_MUTEX_UNLOCK(lock)

#define DEBUG_THREADS_PRINTF(...)
#define DEBUG_PERF_PRINTF(...)

/** Profile Types*/
#define BASE_PROFILE_IDC    66
#define MAIN_PROFILE_IDC    77
#define EXTENDED_PROFILE_IDC    88
#define HIGH_PROFILE_IDC   100


#define MB_SIZE             16
#define BLK8x8SIZE           8
#define BLK_SIZE             4
#define NUM_BLKS_PER_MB     24
#define NUM_LUM_BLKS_PER_MB 16
#define LUM_BLK              0
#define CHROM_BLK            1
#define NUM_PELS_IN_MB      64

/* Level Types */
#define H264_LEVEL_1_0     10
#define H264_LEVEL_1_1     11
#define H264_LEVEL_1_2     12
#define H264_LEVEL_1_3     13
#define H264_LEVEL_2_0     20
#define H264_LEVEL_2_1     21
#define H264_LEVEL_2_2     22
#define H264_LEVEL_3_0     30
#define H264_LEVEL_3_1     31
#define H264_LEVEL_3_2     32
#define H264_LEVEL_4_0     40
#define H264_LEVEL_4_1     41
#define H264_LEVEL_4_2     42
#define H264_LEVEL_5_0     50
#define H264_LEVEL_5_1     51

#define MAX_MBS_LEVEL_51 36864
#define MAX_MBS_LEVEL_50 22080
#define MAX_MBS_LEVEL_42 8704
#define MAX_MBS_LEVEL_41 8192
#define MAX_MBS_LEVEL_40 8192
#define MAX_MBS_LEVEL_32 5120
#define MAX_MBS_LEVEL_31 3600
#define MAX_MBS_LEVEL_30 1620
#define MAX_MBS_LEVEL_22 1620
#define MAX_MBS_LEVEL_21 792
#define MAX_MBS_LEVEL_20 396
#define MAX_MBS_LEVEL_13 396
#define MAX_MBS_LEVEL_12 396
#define MAX_MBS_LEVEL_11 396
#define MAX_MBS_LEVEL_10 99

/** NAL Types */
#define SLICE_NAL                       1
#define SLICE_DATA_PARTITION_A_NAL      2
#define SLICE_DATA_PARTITION_B_NAL      3
#define SLICE_DATA_PARTITION_C_NAL      4
#define IDR_SLICE_NAL                   5
#define SEI_NAL                         6
#define SEQ_PARAM_NAL                   7
#define PIC_PARAM_NAL                   8
#define ACCESS_UNIT_DELIMITER_RBSP      9
#define END_OF_SEQ_RBSP                 10
#define END_OF_STREAM_RBSP              11
#define FILLER_DATA_NAL                 12

/** Entropy coding modes */
#define CAVLC  0
#define CABAC  1

/** Picture Types */
#define I_PIC       0
#define IP_PIC      1
#define IPB_PIC     2
#define SI_PIC      3
#define SIP_PIC     4
#define ISI_PIC     5
#define ISI_PSP_PIC 6
#define ALL_PIC     7

/* Frame or field picture type */
#define FRM_PIC         0x00
#define TOP_FLD         0x01
#define BOT_FLD         0x02
#define COMP_FLD_PAIR   0x03 /* TOP_FLD | BOT_FLD */
#define AFRM_PIC        0x04
#define TOP_REF         0x08
#define BOT_REF         0x10
#define PIC_MASK        0x03
#define NON_EXISTING    0xff

/* field picture type for display */
#define DISP_TOP_FLD    0x00
#define DISP_BOT_FLD    0x01

/** Slice Types */
#define NA_SLICE -1
#define P_SLICE  0
#define B_SLICE  1
#define I_SLICE  2
#define SP_SLICE 3
#define SI_SLICE 4

/* Definition for picture skip */
#define SKIP_NONE  (0x0)
#define I_SLC_BIT  (0x1)
#define P_SLC_BIT  (0x2)
#define B_SLC_BIT  (0x4)

/** Macros used for Deblocking */
#define D_INTER_MB        0
#define D_INTRA_MB        1
#define D_PRED_NON_16x16  2
#define D_B_SLICE         4
#define D_B_SUBMB         6 //D_B_SLICE | D_PRED_NON_16x16 | D_INTER_MB
#define D_FLD_MB          0x80

/** Macros for Cabac checks */
/** MbType */
/** |x|x|I_PCM|SKIP|
 |S|Inter/Intra|P/B|NON-BD16x16/BD16x16,I16x16/I4x4| */
#define CAB_INTRA         0x00 /* 0000 00xx */
#define CAB_INTER         0x04 /* 0000 01xx */
#define CAB_I4x4          0x00 /* 0000 00x0 */
#define CAB_I16x16        0x01 /* 0000 00x1 */
#define CAB_BD16x16       0x04 /* 0000 0100 */
#define CAB_NON_BD16x16   0x05 /* 0000 0101 */
#define CAB_P             0x07 /* 0000 0111 */
#define CAB_SI4x4         0x08 /* 0000 10x0 */
#define CAB_SI16x16       0x09 /* 0000 10x1 */
#define CAB_SKIP_MASK     0x10 /* 0001 0000 */
#define CAB_SKIP          0x10 /* 0001 0000 */
#define CAB_P_SKIP        0x16 /* 0001 x11x */
#define CAB_B_SKIP        0x14 /* 0001 x100 */
#define CAB_BD16x16_MASK  0x07 /* 0000 0111 */
#define CAB_INTRA_MASK    0x04 /* 0000 0100 */
#define CAB_I_PCM         0x20 /* 001x xxxx */

/**< Binarization types for CABAC */
/* |x|x|x|x|MSB_FIRST_FLC|FLC|TUNARY|UNARY| */
#define UNARY           1
#define TUNARY          2
#define FLC             4
#define MSB_FIRST_FLC   12

/** Macroblock Types */
#define I_4x4_MB    0
#define I_16x16_MB  1
#define P_MB        2
#define B_MB        3
#define SI_MB       4
#define SP_MB       5
#define I_PCM_MB    6

#define SI4x4_MB 0xFF

/** Intra luma 16x16 and chroma 8x8 prediction modes */
#define NUM_INTRA_PRED_MODES  4
#define VERT    0
#define HORIZ   1
#define DC      2
#define PLANE   3
#define NOT_VALID -1
#define DC_DC_DC_DC   0x02020202 /*packed 4 bytes used in Decode Intra Mb*/

/** Intra luma 4x4 prediction modes */
#define NUM_INTRA4x4_PRED_MODES 9

/** VERT, HORIZ, DC are applicable to 4x4 as well */
/** D - Down; U - Up; L - Left; R - Right */
#define DIAG_DL   3
#define DIAG_DR   4
#define VERT_R    5
#define HORIZ_D   6
#define VERT_L    7
#define HORIZ_U   8

/** P_MB prediction modes */
#define NUM_INTER_MB_PRED_MODES 5
#define PRED_16x16  0
#define PRED_16x8   1
#define PRED_8x16   2
#define PRED_8x8    3
#define PRED_8x8R0  4
#define MAGIC_16x16 5
#define MB_SKIP     255

/* P_MB submb modes */
#define P_L0_8x8    0
#define P_L0_8x4    1
#define P_L0_4x8    2
#define P_L0_4x4    3

/* B_MB submb modes */
#define B_DIRECT_8x8    0
#define B_L0_8x8        1
#define B_L1_8x8        2
#define B_BI_8x8        3
#define B_L0_8x4        4
#define B_L0_4x8        5
#define B_L1_8x4        6
#define B_L1_4x8        7
#define B_BI_8x4        8
#define B_BI_4x8        9
#define B_L0_4x4        10
#define B_L1_4x4        11
#define B_BI_4x4        12

/** B_MB prediction modes */
#define B_8x8    22
#define PRED_INVALID  -1
#define B_DIRECT  0
#define PRED_L0   1
#define PRED_L1   2
#define BI_PRED   3
#define B_DIRECT_BI_PRED  23
#define B_DIRECT_PRED_L0  24
#define B_DIRECT_PRED_L1  25
#define B_DIRECT_SPATIAL  26

#define B_DIRECT8x8_BI_PRED  13
#define B_DIRECT8x8_PRED_L0  14
#define B_DIRECT8x8_PRED_L1  15

#define ONE_TO_ONE  0
#define FRM_TO_FLD  1
#define FLD_TO_FRM  2

/** Inter Sub MB Pred modes */
#define NUM_INTER_SUBMB_PRED_MODES 4
#define SUBMB_8x8    0
#define SUBMB_8x4    1
#define SUBMB_4x8    2
#define SUBMB_4x4    3

/** Coded Block Pattern - Chroma */
#define CBPC_ALLZERO    0
#define CBPC_ACZERO     1
#define CBPC_NONZERO    2

/** Index for accessing the left MB in the MV predictor array */
#define LEFT  0
/** Index for accessing the top MB in the MV predictor array */
#define TOP   1
/** Index for accessing the top right MB in the MV predictor array */
#define TOP_R 2
/** Index for accessing the top Left MB in the MV predictor array */
#define TOP_L 3

/** Maximum number of Sequence Parameter sets */
#define MAX_NUM_SEQ_PARAMS 32

/** Maximum number of Picture Parameter sets */
#define MAX_NUM_PIC_PARAMS 256

#define MASK_ERR_SEQ_SET_ID   (0xFFFFFFE0)
#define MASK_ERR_PIC_SET_ID   (0xFFFFFF00)

#define MAX_PIC_ORDER_CNT_TYPE    2

#define MAX_BITS_IN_FRAME_NUM     16
#define MAX_BITS_IN_POC_LSB       16

#define H264_MAX_REF_PICS         16
#define H264_MAX_REF_IDX          32
#define MAX_WEIGHT_BIPRED_IDC     2
#define MAX_CABAC_INIT_IDC        2

#define H264_DEFAULT_NUM_CORES 1
#define DEFAULT_SEPARATE_PARSE (H264_DEFAULT_NUM_CORES == 2)? 1 :0

/** Maximum number of Slice groups */
#define MAX_NUM_SLICE_GROUPS 8
#define MAX_NUM_REF_FRAMES_OFFSET 255

/** Deblocking modes for a slice */
#define SLICE_BOUNDARY_DBLK_DISABLED  2
#define DBLK_DISABLED                 1
#define DBLK_ENABLED                  0
#define MIN_DBLK_FIL_OFF              -12
#define MAX_DBLK_FIL_OFF              12

/** Width of the predictor buffers used for MC */
#define MB_SIZE             16
#define BLK8x8SIZE          8
#define BLK_SIZE             4
#define NUM_BLKS_PER_MB     24
#define NUM_LUM_BLKS_PER_MB 16

#define SUB_BLK_WIDTH                 4
#define SUB_SUB_BLK_SIZE              4 /* 2x2 pixel i4_size */
#define SUB_BLK_SIZE                  ((SUB_BLK_WIDTH) * (SUB_BLK_WIDTH))
#define MB_LUM_SIZE                   256
#define MB_CHROM_SIZE                 64

/**< Width to pad the luminance frame buff    */
/**< Height to pad the luminance frame buff   */
/**< Width to pad the chrominance frame buff  */
/**< Height to pad the chrominance frame buff */

#define PAD_LEN_Y_H                   32
#define PAD_LEN_Y_V                   20
#define PAD_LEN_UV_H                  16
#define PAD_LEN_UV_V                  8

#define PAD_MV_BANK_ROW             64

/**< Maimum u4_ofst by which the Mvs could point outside the frame buffers
 horizontally in the left and vertically in the top direction */
#define MAX_OFFSET_OUTSIDE_X_FRM      -20
#define MAX_OFFSET_OUTSIDE_Y_FRM      -20
#define MAX_OFFSET_OUTSIDE_UV_FRM     -8

/** UVLC parsing macros */
#define   UEV     1
#define   SEV     2
#define   TEV     3

/** Defines for Boolean values */
#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#define UNUSED_FOR_REF 0
#define IS_SHORT_TERM  1
#define IS_LONG_TERM   2

/** Defines for which field gets displayed first */
#define MAX_FRAMES              16
#define INVALID_FRAME_NUM       0x0fffffff
#define DO_NOT_DISP             254
#define DISP_FLD_FIRST_UNDEF  0
#define DISP_TOP_FLD_FIRST   1
#define DISP_BOT_FLD_FIRST   2

/** Misc error resilience requirements*/
#define MAX_LOG2_WEIGHT_DENOM       7
#define PRED_WEIGHT_MIN             (-128)
#define PRED_WEIGHT_MAX             127
#define MAX_REDUNDANT_PIC_CNT       127



#endif //DEBLOCK_THREAD

#define NUM_COEFFS_IN_4x4BLK 16
#define CABAC_BITS_TO_READ 23

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


#define MEMSET_16BYTES(pu4_start,value)                         \
{                                                               \
    memset(pu4_start,value,16);                                 \
}

#define MEMCPY_16BYTES(dst,src)                                 \
{                                                               \
    memcpy(dst,src,16);                                         \
}


#endif /*_IH264D_DEFS_H_*/
