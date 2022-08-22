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
*  ih264_list.h
*
* @brief
*  Contains functions for buf queue
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

#ifndef _IH264_LIST_H_
#define _IH264_LIST_H_

typedef struct
{
    /** Pointer to buffer base which contains the bufs */
    void *pv_buf_base;

    /** Mutex used to keep the functions thread-safe */
    void *pv_mutex;

    /** Current write index */
    volatile WORD32 i4_buf_wr_idx;

    /** Current read index */
    volatile WORD32 i4_buf_rd_idx;

    /** Maximum index */
    WORD32 i4_buf_max_idx;

    /** Log2(buf_max_idx) -
     * To ensure number of entries is power of two
     * This makes it easier to wrap around by using AND with buf_max_idx - 1
     * */
    WORD32 i4_log2_buf_max_idx;

    /** Flag to indicate list has to be terminated */
    WORD32 i4_terminate;

    /** Size of each entry */
    WORD32 i4_entry_size;

    /** If the list is to be used frequently send this as zero, else send a large value
     * to ensure cores are not loaded unnecessarily.
     * For eg: For picture level queues this can be a large value like 100us
     * but for jobq this will be zero.
     */
    WORD32 i4_yeild_interval_us;

}list_t;

WORD32 ih264_list_size(WORD32 num_entries, WORD32 entry_size);
void* ih264_list_init(void *pv_buf,
                      WORD32 buf_size,
                      WORD32 num_entries,
                      WORD32 entry_size,
                      WORD32 yeild_interval_us);
IH264_ERROR_T ih264_list_free(list_t *ps_list);
IH264_ERROR_T ih264_list_reset(list_t *ps_list);
IH264_ERROR_T ih264_list_deinit(list_t *ps_list);
IH264_ERROR_T ih264_list_terminate(list_t *ps_list);
IH264_ERROR_T ih264_list_queue(list_t *ps_list, void *pv_buf, WORD32 blocking);
IH264_ERROR_T ih264_list_dequeue(list_t *ps_list, void *pv_buf, WORD32 blocking);

#endif /* _IH264_PROCESS_SLICE_H_ */
