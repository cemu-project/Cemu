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
*  imp2d_function_selector.c
*
* @brief
*  Contains functions to initialize function pointers used in hevc
*
* @author
*  Naveen
*
* @par List of Functions:
* @remarks
*  None
*
*******************************************************************************
*/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* User Include files */
#include "ih264_typedefs.h"
#include "iv.h"
#include "ivd.h"
#include "ih264_defs.h"
#include "ih264_size_defs.h"
#include "ih264_error.h"
#include "ih264_trans_quant_itrans_iquant.h"
#include "ih264_inter_pred_filters.h"

#include "ih264d_structs.h"
#include "ih264d_function_selector.h"

void ih264d_init_function_ptr(dec_struct_t *ps_codec)
{

    ih264d_init_function_ptr_generic(ps_codec);
    switch(ps_codec->e_processor_arch)
    {
        case ARCH_X86_GENERIC:
            ih264d_init_function_ptr_generic(ps_codec);
        break;
        case ARCH_X86_SSSE3:
            ih264d_init_function_ptr_ssse3(ps_codec);
            break;
        case ARCH_X86_SSE42:
        default:
            ih264d_init_function_ptr_ssse3(ps_codec);
            ih264d_init_function_ptr_sse42(ps_codec);
        break;
    }
}

#ifdef __GNUC__
#include <cpuid.h>

void __cpuid2(signed int* cpuInfo, unsigned int level)
{
    __get_cpuid (level, (unsigned int*)cpuInfo+0, (unsigned int*)cpuInfo+1, (unsigned int*)cpuInfo+2, (unsigned int*)cpuInfo+3);
}

#define __getCPUId __cpuid2

#else
#define __getCPUId __cpuid
#endif

void ih264d_init_arch(dec_struct_t *ps_codec)
{
	int cpuInfo[4];
    UWORD8 hasSSSE3;
    UWORD8 hasSSE4_2;
	__getCPUId(cpuInfo, 0x1);
	hasSSSE3 = ((cpuInfo[2] >> 9) & 1);
	hasSSE4_2 = ((cpuInfo[2] >> 20) & 1);
    if (hasSSE4_2 != 0)
        ps_codec->e_processor_arch = ARCH_X86_SSE42;
    else if (hasSSSE3 != 0)
		ps_codec->e_processor_arch = ARCH_X86_SSSE3;
	else
        ps_codec->e_processor_arch = ARCH_X86_GENERIC;
}
