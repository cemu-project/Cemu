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
*  ih264_error.h
*
* @brief
*  Definitions related to error handling for common modules
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

#ifndef _IH264_ERROR_H_
#define _IH264_ERROR_H_

/**
 * Enumerations for error codes used in the codec.
 * Not all these are expected to be returned to the application.
 * Only select few will be exported
 */
typedef enum
{
    /**
     *  No error
     */
    IH264_SUCCESS = 0,
    /**
     *  Start error code for decoder
     */
    IH264_DEC_ERROR_START = 0x100,

    /**
     *  Start error code for encoder
     */
    IH264_ENC_ERROR_START = 0x200,
    /**
     * Generic failure
     */
    IH264_FAIL                             = 0x7FFFFFFF
}IH264_ERROR_T;

#endif /* _IH264_ERROR_H_ */
