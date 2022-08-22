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

/*!
 **************************************************************************
 * \file ih264d_bitstrm.c
 *
 * \brief
 *    Bitstream parsing routines
 *
 * \date
 *    20/11/2002
 *
 * \author  AI
 **************************************************************************
 */

#include <stdlib.h>
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_bitstrm.h"
#include "ih264d_error_handler.h"

#include "ih264d_debug.h"
#include "ih264d_tables.h"
#include "ih264d_structs.h"

/*!
 **************************************************************************
 * \if Function name : ih264d_get_bit_h264 \endif
 *
 * \brief
 *    Read one bit from the bitstream.
 *
 *   This is a Bitstream processing function. It reads the
 *   bit currently pointed by the bit pointer in the
 *   buffer and advances the pointer by one. It returns
 *   the bit (0 or 1) in the form of an unsigned integer.
 *
 * \return
 *    Returns the next bit (0 or 1) in the bitstream.
 *
 **************************************************************************
 */
UWORD8 ih264d_get_bit_h264(dec_bit_stream_t *ps_stream)
{
    UWORD32 u4_code;

    GETBIT(u4_code, ps_stream->u4_ofst, ps_stream->pu4_buffer);
    return (u4_code);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_get_bits_h264 \endif
 *
 * \brief
 *    Read specified number of bits from the bitstream.
 *
 *   This is a Bitstream processing function. It reads the
 *   number specified number of bits from the current bit
 *   position and advances the bit and byte pointers
 *   appropriately.
 *
 * \return
 *    An unsigned 32 bit integer with its least significant bits
 *    containing the bits in order of their occurence in the bitstream.
 *
 **************************************************************************
 */

UWORD32 ih264d_get_bits_h264(dec_bit_stream_t *ps_bitstrm, UWORD32 u4_num_bits)
{
    UWORD32 u4_code = 0;
    if(u4_num_bits)
        GETBITS(u4_code, ps_bitstrm->u4_ofst, ps_bitstrm->pu4_buffer, u4_num_bits);
    return (u4_code);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_next_bits_h264 \endif
 *
 * \brief
 *    Peek specified number of bits from the bitstream.
 *
 *   This is a Bitstream processing function. It gets the
 *   specified number of bits from the buffer without
 *   altering the current pointers. It is equivalent to
 *   next_bits() function in the standard.
 *
 * \return
 *    An unsigned 32 bit integer with its least significant bits
 *    containing the bits in order of their occurence in the bitstream.
 **************************************************************************
 */
UWORD32 ih264d_next_bits_h264(dec_bit_stream_t *ps_bitstrm, UWORD32 u4_num_bits)
{
    UWORD32 u4_word_off = (ps_bitstrm->u4_ofst >> 5);
    UWORD32 u4_bit_off = ps_bitstrm->u4_ofst & 0x1F;
    UWORD32 *pu4_bitstream = ps_bitstrm->pu4_buffer;
    UWORD32 u4_bits = pu4_bitstream[u4_word_off++] << u4_bit_off;

    /*************************************************************************/
    /* Test if number of bits to be read exceeds the number of bits in the   */
    /* current word. If yes, read from the next word of the buffer, The bits */
    /* from both the words are concatenated to get next 32 bits in 'u4_bits' */
    /*************************************************************************/
    if(u4_bit_off > (INT_IN_BITS - u4_num_bits))
        u4_bits |= (pu4_bitstream[u4_word_off] >> (INT_IN_BITS - u4_bit_off));

    return ((u4_bits >> (INT_IN_BITS - u4_num_bits)));
}

/*!
 **************************************************************************
 * \if Function name : ih264d_flush_bits_h264 \endif
 *
 * \brief
 *    Flush specified number of bits from the bitstream.
 *
 *   This function flushes the specified number of bits (marks
 *   as read) from the buffer.
 *
 * \return
 *     A 8 bit unsigned integer with value
 *    '1' on successful flush
 *    '0' on failure.
 *
 **************************************************************************
 */
WORD32 ih264d_flush_bits_h264(dec_bit_stream_t *ps_bitstrm, WORD32 u4_num_bits)
{
    ps_bitstrm->u4_ofst += u4_num_bits;

    if(ps_bitstrm->u4_ofst > ps_bitstrm->u4_max_ofst)
    {
        return ERROR_EOB_FLUSHBITS_T;
    }
    return OK;
}

/*!
 **************************************************************************
 * \if Function name : ih264d_check_byte_aligned \endif
 *
 * \brief
 *    Checks whether the bit ps_bitstrm u4_ofst is at byte boundary.
 *
 * \param ps_bitstrm : Pointer to bitstream
 *
 * \return
 *    Returns 1 if bit ps_bitstrm u4_ofst is at byte alligned position else zero.
 **************************************************************************
 */

UWORD8 ih264d_check_byte_aligned(dec_bit_stream_t * ps_bitstrm)
{
    if(ps_bitstrm->u4_ofst & 0x07)
        return (0);
    else
        return (1);
}
