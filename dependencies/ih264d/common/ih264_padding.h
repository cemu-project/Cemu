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
*  ih264_padding.h
*
* @brief
*  Declarations for padding functions
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef _IH264_PADDING_H_
#define _IH264_PADDING_H_

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/

typedef void ih264_pad(UWORD8 *, WORD32, WORD32, WORD32);

/* C function declarations */
ih264_pad ih264_pad_top;
ih264_pad ih264_pad_bottom;
ih264_pad ih264_pad_left_luma;
ih264_pad ih264_pad_left_chroma;
ih264_pad ih264_pad_right_luma;
ih264_pad ih264_pad_right_chroma;

/* A9 Q function declarations */
ih264_pad ih264_pad_top_a9q;
ih264_pad ih264_pad_left_luma_a9q;
ih264_pad ih264_pad_left_chroma_a9q;
ih264_pad ih264_pad_right_luma_a9q;
ih264_pad ih264_pad_right_chroma_a9q;

/* AV8 function declarations */
ih264_pad ih264_pad_top_av8;
ih264_pad ih264_pad_left_luma_av8;
ih264_pad ih264_pad_left_chroma_av8;
ih264_pad ih264_pad_right_luma_av8;
ih264_pad ih264_pad_right_chroma_av8;


ih264_pad ih264_pad_left_luma_ssse3;
ih264_pad ih264_pad_left_chroma_ssse3;
ih264_pad ih264_pad_right_luma_ssse3;
ih264_pad ih264_pad_right_chroma_ssse3;

#endif /*_IH264_PADDING_H_*/
