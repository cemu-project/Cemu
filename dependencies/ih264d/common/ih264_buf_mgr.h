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
*  ih264_buf_mgr.h
*
* @brief
*  Function declarations used for buffer management
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef _IH264_BUF_MGR_H_
#define _IH264_BUF_MGR_H_

#define BUF_MGR_MAX_CNT 64

/** Flag for current encoding decoder */
#define BUF_MGR_CODEC        (1 << 1)

/** Flag for reference status */
#define BUF_MGR_REF          (1 << 2)

/** Flag for I/O - Display/output in case of decoder, capture/input in case of encoder */
#define BUF_MGR_IO           (1 << 3)

typedef struct
{
    /**
     * Mutex used to keep the functions thread-safe
     */
    void *pv_mutex;

    /**
     * max_buf_cnt
     */
    WORD32 i4_max_buf_cnt;

    /**
     * active_buf_cnt
     */
    WORD32 i4_active_buf_cnt;

    /**
     *  au4_status[BUF_MGR_MAX_CNT]
     */
    UWORD32 au4_status[BUF_MGR_MAX_CNT];

    /* The last three bit of status are:    */

    /* Bit 0 - IN USE                       */
    /* Bit 1 - CODEC                        */
    /* Bit 2 - REF                          */
    /* Bit 3 - DISP/IO/RECON                */
    void    *apv_ptr[BUF_MGR_MAX_CNT];

}buf_mgr_t;

// Returns size of the buffer manager context
WORD32 ih264_buf_mgr_size(void);

//Free buffer manager
IH264_ERROR_T ih264_buf_mgr_free(buf_mgr_t *ps_buf_mgr);

// Initializes the buffer API structure
void *ih264_buf_mgr_init(void *pv_buf);

// Add buffer to buffer manager. 0: success, -1: fail (u4_active_buf_cnt has reached u4_max_buf_cnt)
IH264_ERROR_T ih264_buf_mgr_add(buf_mgr_t *ps_buf_mgr,
                                void *pv_ptr,
                                WORD32 buf_id);

// this function will set the buffer status to DEC
void* ih264_buf_mgr_get_next_free(buf_mgr_t *ps_buf_mgr, WORD32 *pi4_id);

// this function will check if there are any free buffers
IH264_ERROR_T ih264_buf_mgr_check_free(buf_mgr_t *ps_buf_mgr);

// mask will have who released it: DISP:REF:DEC
IH264_ERROR_T ih264_buf_mgr_release(buf_mgr_t *ps_buf_mgr,
                                    WORD32 id,
                                    UWORD32 mask);

// sets the status to one or all of DISP:REF:DEC
IH264_ERROR_T ih264_buf_mgr_set_status(buf_mgr_t *ps_buf_mgr,
                                       WORD32 id,
                                       UWORD32 mask);

// Gets status of the buffer
WORD32 ih264_buf_mgr_get_status(buf_mgr_t *ps_buf_mgr, WORD32 id);

// pass the ID - buffer will be returned
void* ih264_buf_mgr_get_buf(buf_mgr_t *ps_buf_mgr, WORD32 id);
//Pass buffer to get ID
WORD32 ih264_buf_mgr_get_bufid(buf_mgr_t *ps_buf_mgr, void *pv_buf);

// will return number of active buffers
UWORD32 ih264_buf_mgr_get_num_active_buf(buf_mgr_t *ps_buf_mgr);



#endif  /* _IH264_BUF_MGR_H_ */
