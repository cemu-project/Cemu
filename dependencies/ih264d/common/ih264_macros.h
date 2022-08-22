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

/*********************************************************************************
* @file
*  ih264_macros.h
*
* @brief
*  Macro definitions used in the codec
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef _IH264_MACROS_H_
#define _IH264_MACROS_H_

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define RETURN_IF(cond, retval) if(cond) {return (retval);}
#define UNUSED(x) ((void)(x))

#define ALIGN128(x) ((((x) + 127) >> 7) << 7)
#define ALIGN64(x)  ((((x) + 63) >> 6) << 6)
#define ALIGN32(x)  ((((x) + 31) >> 5) << 5)
#define ALIGN16(x)  ((((x) + 15) >> 4) << 4)
#define ALIGN8(x)   ((((x) + 7) >> 3) << 3)
#define ALIGN4(x)   ((((x) + 3) >> 2) << 2)
#define ALIGN2(x)   ((((x) + 1) >> 1) << 1)

/**
******************************************************************************
 *  @brief      Min, Max
******************************************************************************
 */
#define MAX(a,b) ((a > b)?(a):(b))
#define MIN(a,b) ((a < b)?(a):(b))
#define MIN3(a,b,c) ((a) < (b)) ? (((a) < (c)) ? (a) : (c)) : (((b) < (c)) ? (b) : (c))
#define MAX3(a,b,c) ((a) > (b)) ? (((a) > (c)) ? (a) : (c)) : (((b) > (c)) ? (b) : (c))
/**
******************************************************************************
 *  @brief      Div, Mod
******************************************************************************
 */
#define MOD(x,y) ((x)%(y))
#define DIV(x,y) ((x)/(y))

/**
******************************************************************************
 *  @brief      Clip
******************************************************************************
 */
#define CLIP3(miny, maxy, y) (((y) < (miny))?(miny):(((y) > (maxy))?(maxy):(y)))

/**
******************************************************************************
 *  @brief      True, False
******************************************************************************
 */
#define BOOLEAN(x) (!!(x))

/**
******************************************************************************
 *  @brief      Frequently used multiplications x2. x3, and x4
******************************************************************************
 */
#define X2(a)   ((a) << 1)
#define X3(a)   (((a) << 1) + (a))
#define X4(a)   ((a) << 2)

/**
******************************************************************************
 *  @brief      Misc
******************************************************************************
 */
#define ABS(x)          ((x) < 0 ? (-(x)) : (x))
#define SIGNXY(x,y)     (((y) < 0) ? (-1 * (x)) : (x))

#define SIGN(x)     (((x) >= 0) ? (((x) > 0) ? 1 : 0) : -1)

#define RESET_BIT(x, pos) (x) = (x) & ~(1 << pos);
#define SET_BIT(x, pos) (x) = (x) | (1 << pos);
#define GET_BIT(x, pos) ((x) >> (pos)) & 0x1

#define INSERT_BIT(x, pos, bit) { RESET_BIT(x, pos); (x) = (x) | (bit << pos); }
#endif /*_IH264_MACROS_H_*/


