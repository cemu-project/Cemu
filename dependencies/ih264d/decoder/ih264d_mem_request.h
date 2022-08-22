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

#ifndef _IH264D_MEM_REQUEST_H_
#define _IH264D_MEM_REQUEST_H_
/*!
 ***************************************************************************
 * \file ih264d_mem_request.h
 *
 * \brief
 *    This file contains declarations and data structures of the API's which
 *    required to interact with Picture Buffer.
 *
 *
 * \date
 *    11/12/2002
 *
 * \author  NS
 ***************************************************************************/
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_defs.h"
#include "ih264d_structs.h"

#define MAX_MEM_BLOCKS      64 + 8

struct MemBlock
{
    void ** v_memLocation; /** memory location where address of allocated memory should be stored*/
    UWORD32 u4_mem_size; /** Size of the memory block */
};

struct MemReq
{
    UWORD32 u4_num_memBlocks; /** Number of memory blocks */
    struct MemBlock s_memBlock[MAX_MEM_BLOCKS]; /** Pointer to the first memory block */
};

struct PicMemBlock
{
    void * buf1; /** memory location for buf1 */
    void * buf2; /** memory location for buf2 */
    void * buf3; /** memory location for buf3 */
};

struct PicMemReq
{
    WORD32 i4_num_pic_memBlocks; /** Number of memory blocks */
    UWORD32 u4_size1; /** Size of the buf1 in PicMemBlock */
    UWORD32 u4_size2; /** Size of the buf2 in PicMemBlock */
    UWORD32 u4_size3; /** Size of the buf3 in PicMemBlock */
    struct PicMemBlock s_PicMemBlock[MAX_DISP_BUFS_NEW];
};

WORD32 ih264d_create_pic_buffers(UWORD8 u1_num_of_buf,
                               dec_struct_t *ps_dec);

WORD32 ih264d_create_mv_bank(void * pv_codec_handle,
                             UWORD32 u4_wd,
                             UWORD32 u4_ht);
WORD16 ih264d_allocate_dynamic_bufs(dec_struct_t * ps_dec);


#endif  /* _IH264D_MEM_REQUEST_H_ */
