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
#ifndef _IH264D_DEBUG_H_
#define _IH264D_DEBUG_H_

/*!
 **************************************************************************
 * \file ih264d_debug.h
 *
 * \brief
 *    Contains declarations used for debugging
 *
 * \date
 *    2/12/2002
 *
 * \author  AI
 **************************************************************************
 */
#ifdef DEBUG_DEC
#define H264_DEC_DEBUG_PRINT(...) printf("\n[H264_DEBUG] %s/%d:: ", __FUNCTION__, __LINE__);printf(__VA_ARGS__)
#else //DEBUG_DEC
#define H264_DEC_DEBUG_PRINT(...) {}
#endif //DEBUG_DEC
#define   STRENGTH_DEBLOCKING         0 //sanjeev
#define DEBUG_RECONSTRUCT_LUMA    0
#define DEBUG_RECONSTRUCT_CHROMA  0

#define DEBUG_IDCT                0
#define DEBUG_LUMA_IDCT           0
#define DEBUG_REF_IDCT            0

#define BIN_BIT_RATIO             0
#define MB_PART_HIST              0

#define MB_INTRA_PREDICTION       1

#ifdef WIN32
#define CHK_PURIFY                0
#else
#define CHK_PURIFY                0
#endif

#if MB_INTRA_PREDICTION
#define MB_INTRA_CHROMA_PREDICTION_ON 1
#define MB_INTRA_4x4_PREDICTION_ON    1
#define MB_INTRA_16x16_PREDICTION_ON  1
#endif

#define   TRACE                   0
#define   DEBUG_CABAC             0
#define   DEBUG_ABS_MVD           0
#define   DEBUG_INTRA_PRED_MODES  0
#define   DEBUG_DEBLOCKING        0

#define COPYTHECONTEXT(s,val)
#define PRINT_TRACE
#define PRINT_TRACE_CAB
#define SWITCHOFFTRACE
#define SWITCHONTRACE
#define SWITCHOFFTRACECABAC
#define SWITCHONTRACECABAC

#define INC_BIN_COUNT(ps_cab_env)
#define INC_DECISION_BINS(ps_cab_env)
#define INC_BYPASS_BINS(ps_cab_env)
#define INC_SYM_COUNT(ps_cab_env)
#define PRINT_BIN_BIT_RATIO(ps_dec)
#define RESET_BIN_COUNTS(ps_cab_env)


#ifdef PROFILE_DIS_DEBLK
#define PROFILE_DISABLE_DEBLK() return;
#else
#define PROFILE_DISABLE_DEBLK() ;
#endif

#ifdef PROFILE_DIS_IQ_IT_RECON
#define PROFILE_DISABLE_IQ_IT_RECON()   if (0)
#define PROFILE_DISABLE_IQ_IT_RECON_RETURN()   return;
#else
#define PROFILE_DISABLE_IQ_IT_RECON() ;
#define PROFILE_DISABLE_IQ_IT_RECON_RETURN() ;
#endif

#ifdef PROFILE_DIS_INTRA_PRED
#define PROFILE_DISABLE_INTRA_PRED() if (0)
#else
#define PROFILE_DISABLE_INTRA_PRED() ;
#endif

#ifdef PROFILE_DIS_UNPACK
#define PROFILE_DISABLE_UNPACK_LUMA() return 0;
#define PROFILE_DISABLE_UNPACK_CHROMA() return ;
#else
#define PROFILE_DISABLE_UNPACK_LUMA() ;
#define PROFILE_DISABLE_UNPACK_CHROMA() ;
#endif

#ifdef PROFILE_DIS_INTER_PRED
#define PROFILE_DISABLE_INTER_PRED() return;
#else
#define PROFILE_DISABLE_INTER_PRED() ;
#endif

#ifdef PROFILE_DIS_BOUNDARY_STRENGTH
#define PROFILE_DISABLE_BOUNDARY_STRENGTH() return;
#else
#define PROFILE_DISABLE_BOUNDARY_STRENGTH() ;
#endif

#ifdef PROFILE_DIS_MB_PART_INFO
#define PROFILE_DISABLE_MB_PART_INFO() return 0;
#else
#define PROFILE_DISABLE_MB_PART_INFO() ;
#endif

#endif /* _IH264D_DEBUG_H_ */

