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
*  ih264_disp_mgr.c
*
* @brief
*  Contains function definitions for display management
*
* @author
*  Srinivas T
*
* @par List of Functions:
*   - ih264_disp_mgr_init()
*   - ih264_disp_mgr_add()
*   - ih264_disp_mgr_get()
*
* @remarks
*  None
*
*******************************************************************************
*/
#include <stdlib.h>
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_disp_mgr.h"


/**
*******************************************************************************
*
* @brief
*    Initialization function for display buffer manager
*
* @par Description:
*    Initializes the display buffer management structure
*
* @param[in] ps_disp_mgr
*  Pointer to the display buffer management structure
*
* @returns none
*
* @remarks
*  None
*
*******************************************************************************
*/
void ih264_disp_mgr_init(disp_mgr_t *ps_disp_mgr)
{
    WORD32 id;

    ps_disp_mgr->u4_last_abs_poc = DEFAULT_POC;

    for(id = 0; id < DISP_MGR_MAX_CNT; id++)
    {
        ps_disp_mgr->ai4_abs_poc[id] = DEFAULT_POC;
        ps_disp_mgr->apv_ptr[id] = NULL;
    }
}


/**
*******************************************************************************
*
* @brief
*     Adds a buffer to the display manager
*
* @par Description:
*      Adds a buffer to the display buffer manager
*
* @param[in] ps_disp_mgr
*  Pointer to the display buffer management structure
*
* @param[in] buf_id
*  ID of the display buffer
*
* @param[in] abs_poc
*  Absolute POC of the display buffer
*
* @param[in] pv_ptr
*  Pointer to the display buffer
*
* @returns  0 if success, -1 otherwise
*
* @remarks
*  None
*
*******************************************************************************
*/
WORD32 ih264_disp_mgr_add(disp_mgr_t *ps_disp_mgr,
                          WORD32 buf_id,
                          WORD32 abs_poc,
                          void *pv_ptr)
{
    if(buf_id >= DISP_MGR_MAX_CNT)
    {
        return (-1);
    }

    if(ps_disp_mgr->apv_ptr[buf_id] != NULL)
    {
        return (-1);
    }

    ps_disp_mgr->apv_ptr[buf_id] = pv_ptr;
    ps_disp_mgr->ai4_abs_poc[buf_id] = abs_poc;
    return 0;
}


/**
*******************************************************************************
*
* @brief
*  Gets the next buffer
*
* @par Description:
*  Gets the next display buffer
*
* @param[in] ps_disp_mgr
*  Pointer to the display buffer structure
*
* @param[out]  pi4_buf_id
*  Pointer to hold buffer id of the display buffer being returned
*
* @returns  Pointer to the next display buffer
*
* @remarks
*  None
*
*******************************************************************************
*/
void* ih264_disp_mgr_get(disp_mgr_t *ps_disp_mgr, WORD32 *pi4_buf_id)
{
    WORD32 id;
    void *pv_ret_ptr;
    WORD32 i4_min_poc;
    WORD32 min_poc_id;


    pv_ret_ptr = NULL;
    i4_min_poc = 0x7FFFFFFF;
    min_poc_id = -1;

    /* Find minimum POC */
    for(id = 0; id < DISP_MGR_MAX_CNT; id++)
    {
        if((DEFAULT_POC != ps_disp_mgr->ai4_abs_poc[id]) &&
           (ps_disp_mgr->ai4_abs_poc[id] <= i4_min_poc))
        {
            i4_min_poc = ps_disp_mgr->ai4_abs_poc[id];
            min_poc_id = id;
        }
    }
    *pi4_buf_id = min_poc_id;
    /* If all pocs are still default_poc then return NULL */
    if(-1 == min_poc_id)
    {
        return NULL;
    }

    pv_ret_ptr = ps_disp_mgr->apv_ptr[min_poc_id];

    /* Set abs poc to default and apv_ptr to null so that the buffer is not returned again */
    ps_disp_mgr->apv_ptr[min_poc_id] = NULL;
    ps_disp_mgr->ai4_abs_poc[min_poc_id] = DEFAULT_POC;
    return pv_ret_ptr;
}
