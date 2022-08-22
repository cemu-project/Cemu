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
 *  ih264d_function_selector.h
 *
 * @brief
 *  Structure definitions used in the decoder
 *
 * @author
 *  Harish
 *
 * @par List of Functions:
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

#ifndef _IH264D_FUNCTION_SELECTOR_H_
#define _IH264D_FUNCTION_SELECTOR_H_

#define D_ARCH_NA                   1
#define D_ARCH_ARM_NONEON           2
#define D_ARCH_ARM_A9Q              3
#define D_ARCH_ARM_A9A              4
#define D_ARCH_ARM_A9               5
#define D_ARCH_ARM_A7               6
#define D_ARCH_ARM_A5               7
#define D_ARCH_ARM_A15              8
#define D_ARCH_ARM_NEONINTR         9
#define D_ARCH_ARMV8_GENERIC        10
#define D_ARCH_X86_GENERIC          11
#define D_ARCH_X86_SSSE3            12
#define D_ARCH_X86_SSE42            13
#define D_ARCH_X86_AVX2             14
#define D_ARCH_MIPS_GENERIC         15
#define D_ARCH_MIPS_32              16

void ih264d_init_arch(dec_struct_t *ps_codec);

void ih264d_init_function_ptr(dec_struct_t *ps_codec);

void ih264d_init_function_ptr_generic(dec_struct_t *ps_codec);
void ih264d_init_function_ptr_ssse3(dec_struct_t *ps_codec);
void ih264d_init_function_ptr_sse42(dec_struct_t *ps_codec);

void ih264d_init_function_ptr_a9q(dec_struct_t *ps_codec);
void ih264d_init_function_ptr_av8(dec_struct_t *ps_codec);

#endif /* _IH264D_FUNCTION_SELECTOR_H_ */
