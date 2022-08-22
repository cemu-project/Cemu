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
*  ih264_mem_fns.h
*
* @brief
*  Function declarations used for memory functions
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef _IH264_MEM_FNS_H_
#define _IH264_MEM_FNS_H_

typedef void ih264_memcpy_ft(UWORD8 *pu1_dst, UWORD8 *pu1_src, UWORD32 num_bytes);

typedef void ih264_memcpy_mul_8_ft(UWORD8 *pu1_dst, UWORD8 *pu1_src, UWORD32 num_bytes);
/**
 *******************************************************************************
 *
 * @brief
 *   memset of a 8,16 or 32 bytes
 *
 * @par Description:
 *   Does memset of 8bit data for 8,16 or 32 number of bytes
 *
 * @param[in] pu1_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] value
 *  UWORD8 value used for memset
 *
 * @param[in] num_bytes
 *  number of bytes to set
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
typedef void ih264_memset_ft(UWORD8 *pu1_dst, UWORD8 value, UWORD32 num_bytes);

typedef void ih264_memset_mul_8_ft(UWORD8 *pu1_dst, UWORD8 value, UWORD32 num_bytes);

/**
 *******************************************************************************
 *
 * @brief
 *   memset of 16bit data of a 8,16 or 32 bytes
 *
 * @par Description:
 *   Does memset of 16bit data for 8,16 or 32 number of bytes
 *
 * @param[in] pu2_dst
 *  UWORD8 pointer to the destination
 *
 * @param[in] value
 *  UWORD16 value used for memset
 *
 * @param[in] num_words
 *  number of words to set
 * @returns
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
typedef void ih264_memset_16bit_ft(UWORD16 *pu2_dst, UWORD16 value, UWORD32 num_words);

typedef void ih264_memset_16bit_mul_8_ft(UWORD16 *pu2_dst, UWORD16 value, UWORD32 num_words);

/* C function declarations */
ih264_memcpy_ft ih264_memcpy;
ih264_memcpy_mul_8_ft ih264_memcpy_mul_8;
ih264_memset_ft ih264_memset;
ih264_memset_mul_8_ft ih264_memset_mul_8;
ih264_memset_16bit_ft ih264_memset_16bit;
ih264_memset_16bit_mul_8_ft ih264_memset_16bit_mul_8;

/* A9 Q function declarations */
ih264_memcpy_ft ih264_memcpy_a9q;
ih264_memcpy_mul_8_ft ih264_memcpy_mul_8_a9q;
ih264_memset_ft ih264_memset_a9q;
ih264_memset_mul_8_ft ih264_memset_mul_8_a9q;
ih264_memset_16bit_ft ih264_memset_16bit_a9q;
ih264_memset_16bit_mul_8_ft ih264_memset_16bit_mul_8_a9q;

/* AV8 function declarations */
ih264_memcpy_ft ih264_memcpy_av8;
ih264_memcpy_mul_8_ft ih264_memcpy_mul_8_av8;
ih264_memset_ft ih264_memset_av8;
ih264_memset_mul_8_ft ih264_memset_mul_8_av8;
ih264_memset_16bit_ft ih264_memset_16bit_av8;
ih264_memset_16bit_mul_8_ft ih264_memset_16bit_mul_8_av8;


ih264_memcpy_mul_8_ft ih264_memcpy_mul_8_ssse3;
ih264_memset_mul_8_ft ih264_memset_mul_8_ssse3;
ih264_memset_16bit_mul_8_ft ih264_memset_16bit_mul_8_ssse3;
#endif  //_MEM_FNS_H_
