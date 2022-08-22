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
 *  \file   ih264d_nal.c
 *
 *  \brief  NAL parsing routines
 *
 *  Detailed_description
 *
 *  \author
 *         - AI  19 11 2002  Creation
 **************************************************************************
 */
#include "ih264d_bitstrm.h"
#include "ih264d_defs.h"
#include "ih264_typedefs.h"
#include "ih264_macros.h"
#include "ih264_platform_macros.h"
#include "ih264d_defs.h"
#define NUM_OF_ZERO_BYTES_BEFORE_START_CODE 2
#define EMULATION_PREVENTION_BYTE           0x03

#define NAL_FIRST_BYTE_SIZE 1

/*!
 **************************************************************************
 * \if Function name : ih264d_find_start_code \endif
 *
 * \brief
 *    This function searches for the Start Code Prefix.
 *
 * \param pu1_buf : Pointer to char buffer which contains bitstream.
 * \param u4_cur_pos : Current position in the buffer.
 * \param u4_max_ofst : Number of bytes in Buffer.
 * \param pu4_length_of_start_code  : Poiter to length of Start Code.
 *
 * \return
 *    Returns 0 on success and -1 on error.
 *
 **************************************************************************
 */
#define START_CODE_NOT_FOUND    -1
#define END_OF_STREAM_BUFFER    -2
#define END_OF_STREAM           -1

void ih264d_check_if_aud(UWORD8 *pu1_buf,
                         UWORD32 u4_cur_pos,
                         UWORD32 u4_max_ofst,
                         UWORD32 *pu4_next_is_aud)
{
    UWORD8 u1_first_byte, u1_nal_unit_type;
    if(u4_cur_pos + 1 < u4_max_ofst)
    {
        u1_first_byte = pu1_buf[u4_cur_pos + 1];
        u1_nal_unit_type = NAL_UNIT_TYPE(u1_first_byte);

        if(u1_nal_unit_type == ACCESS_UNIT_DELIMITER_RBSP)
        {
            *pu4_next_is_aud = 1;
        }
    }

}
WORD32 ih264d_find_start_code(UWORD8 *pu1_buf,
                              UWORD32 u4_cur_pos,
                              UWORD32 u4_max_ofst,
                              UWORD32 *pu4_length_of_start_code,
                              UWORD32 *pu4_next_is_aud)
{
    WORD32 zero_byte_cnt = 0;
    UWORD32 ui_curPosTemp;

    *pu4_length_of_start_code = 0;
    /*Find first start code */
    while(u4_cur_pos < u4_max_ofst)
    {
        if(pu1_buf[u4_cur_pos] == 0)
            zero_byte_cnt++;
        else if(pu1_buf[u4_cur_pos]
                        == 0x01 && zero_byte_cnt >= NUM_OF_ZERO_BYTES_BEFORE_START_CODE)
        {
            /* Found the start code */
            u4_cur_pos++;
            break;
        }
        else
        {
            zero_byte_cnt = 0;
        }
        u4_cur_pos++;
    }
    /*Find Next Start Code */
    *pu4_length_of_start_code = u4_cur_pos;
    zero_byte_cnt = 0;
    ui_curPosTemp = u4_cur_pos;
    while(u4_cur_pos < u4_max_ofst)
    {

        if(pu1_buf[u4_cur_pos] == 0)
            zero_byte_cnt++;
        else if(pu1_buf[u4_cur_pos]
                        == 0x01 && zero_byte_cnt >= NUM_OF_ZERO_BYTES_BEFORE_START_CODE)
        {
            /* Found the start code */
            ih264d_check_if_aud(pu1_buf, u4_cur_pos, u4_max_ofst,
                                pu4_next_is_aud);
            return (u4_cur_pos - zero_byte_cnt - ui_curPosTemp);
        }
        else
        {
            zero_byte_cnt = 0;
        }
        u4_cur_pos++;
    }

    return (u4_cur_pos - zero_byte_cnt - ui_curPosTemp); //(START_CODE_NOT_FOUND);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_get_next_nal_unit \endif
 *
 * \brief
 *    This function reads one NAl unit.
 *
 * \param ps_nalStream : Poiter to NalUnitStream structure.
 * \param ps_nalUnit : Pointer to NalUnit.
 *
 * \return
 *    Returns 0 on success and -1 on error.
 *
 **************************************************************************
 */
WORD32 ih264d_get_next_nal_unit(UWORD8 *pu1_buf,
                                UWORD32 u4_cur_pos,
                                UWORD32 u4_max_ofst,
                                UWORD32 *pu4_length_of_start_code)
{

    WORD32 i_length_of_nal_unit = 0;
    UWORD32 u4_next_is_aud;

    /* NAL Thread starts */

    ih264d_find_start_code(pu1_buf, u4_cur_pos, u4_max_ofst,
                           pu4_length_of_start_code, &u4_next_is_aud);

    return (i_length_of_nal_unit);
}

/*!
 **************************************************************************
 * \if Function name : ih264d_process_nal_unit \endif
 *
 * \brief
 *    This function removes emulation byte "0x03" from bitstream (EBSP to RBSP).
 *    It also converts bytestream format into 32 bit little-endian format.
 *
 * \param ps_bitstrm : Poiter to dec_bit_stream_t structure.
 * \param pu1_nal_unit  : Pointer to char buffer of NalUnit.
 * \param u4_numbytes_in_nal_unit : Number bytes in NalUnit buffer.
 *
 * \return
 *    Returns number of bytes in RBSP ps_bitstrm.
 *
 * \note
 *    This function is same as nal_unit() of 7.3.1. Apart from nal_unit()
 *    implementation it converts char buffer into 32 bit Buffer. This
 *    facilitates efficient access of bitstream. This has been done taking
 *    into account present processor architectures.
 *
 **************************************************************************
 */
WORD32 ih264d_process_nal_unit(dec_bit_stream_t *ps_bitstrm,
                            UWORD8 *pu1_nal_unit,
                            UWORD32 u4_numbytes_in_nal_unit)
{
    UWORD32 u4_num_bytes_in_rbsp;
    UWORD8 u1_cur_byte;
    WORD32 i = 0;
    WORD8 c_count;
    UWORD32 ui_word;
    UWORD32 *puc_bitstream_buffer = (UWORD32*)pu1_nal_unit;
    ps_bitstrm->pu4_buffer = puc_bitstream_buffer;

    /*--------------------------------------------------------------------*/
    /* First Byte of the NAL Unit                                         */
    /*--------------------------------------------------------------------*/

    ui_word = *pu1_nal_unit++;

    /*--------------------------------------------------------------------*/
    /* Convertion of the EBSP to RBSP                                     */
    /* ie Remove the emulation_prevention_byte [equal to 0x03]            */
    /*--------------------------------------------------------------------*/
    u4_num_bytes_in_rbsp = 0;
    c_count = 0;

//first iteration

    u1_cur_byte = *pu1_nal_unit++;

    ui_word = ((ui_word << 8) | u1_cur_byte);

    c_count++;
    if(u1_cur_byte != 0x00)
        c_count = 0;

//second iteration

    u1_cur_byte = *pu1_nal_unit++;

    ui_word = ((ui_word << 8) | u1_cur_byte);
    u4_num_bytes_in_rbsp = 2;

    c_count++;
    if(u1_cur_byte != 0x00)
        c_count = 0;

    if(u4_numbytes_in_nal_unit > 2)
    {
        i = ((u4_numbytes_in_nal_unit - 3));
    }

    for(; i > 8; i -= 4)
    {

// loop 0
        u1_cur_byte = *pu1_nal_unit++;

        if(c_count == NUM_OF_ZERO_BYTES_BEFORE_START_CODE
                        && u1_cur_byte == EMULATION_PREVENTION_BYTE)
        {
            c_count = 0;
            u1_cur_byte = *pu1_nal_unit++;
            i--;
        }

        ui_word = ((ui_word << 8) | u1_cur_byte);
        *puc_bitstream_buffer = ui_word;
        puc_bitstream_buffer++;
        c_count++;
        if(u1_cur_byte != 0x00)
            c_count = 0;

// loop 1
        u1_cur_byte = *pu1_nal_unit++;

        if(c_count == NUM_OF_ZERO_BYTES_BEFORE_START_CODE
                        && u1_cur_byte == EMULATION_PREVENTION_BYTE)
        {
            c_count = 0;
            u1_cur_byte = *pu1_nal_unit++;
            i--;
        }
        ui_word = ((ui_word << 8) | u1_cur_byte);

        c_count++;
        if(u1_cur_byte != 0x00)
            c_count = 0;

// loop 2
        u1_cur_byte = *pu1_nal_unit++;

        if(c_count == NUM_OF_ZERO_BYTES_BEFORE_START_CODE
                        && u1_cur_byte == EMULATION_PREVENTION_BYTE)
        {
            c_count = 0;
            u1_cur_byte = *pu1_nal_unit++;
            i--;
        }

        ui_word = ((ui_word << 8) | u1_cur_byte);

        c_count++;
        if(u1_cur_byte != 0x00)
            c_count = 0;

// loop 3
        u1_cur_byte = *pu1_nal_unit++;

        if(c_count == NUM_OF_ZERO_BYTES_BEFORE_START_CODE
                        && u1_cur_byte == EMULATION_PREVENTION_BYTE)
        {
            c_count = 0;
            u1_cur_byte = *pu1_nal_unit++;
            i--;
        }

        ui_word = ((ui_word << 8) | u1_cur_byte);

        c_count++;
        if(u1_cur_byte != 0x00)
            c_count = 0;

        u4_num_bytes_in_rbsp += 4;

    }

    for(; i > 0; i--)
    {
        u1_cur_byte = *pu1_nal_unit++;

        if(c_count == NUM_OF_ZERO_BYTES_BEFORE_START_CODE
                        && u1_cur_byte == EMULATION_PREVENTION_BYTE)
        {
            c_count = 0;
            i--;
            u1_cur_byte = *pu1_nal_unit++;
        }

        ui_word = ((ui_word << 8) | u1_cur_byte);
        u4_num_bytes_in_rbsp++;

        if((u4_num_bytes_in_rbsp & 0x03) == 0x03)
        {
            *puc_bitstream_buffer = ui_word;
            puc_bitstream_buffer++;
        }
        c_count++;
        if(u1_cur_byte != 0x00)
            c_count = 0;

    }

    *puc_bitstream_buffer = (ui_word
                    << ((3 - (((u4_num_bytes_in_rbsp << 30) >> 30))) << 3));
    ps_bitstrm->u4_ofst = 0;
    ps_bitstrm->u4_max_ofst = ((u4_num_bytes_in_rbsp + NAL_FIRST_BYTE_SIZE) << 3);

    return (u4_num_bytes_in_rbsp);
}


/*!
 **************************************************************************
 * \if Function name : ih264d_rbsp_to_sodb \endif
 *
 * \brief
 *    This function converts RBSP to SODB.
 *
 * \param ps_bitstrm : Poiter to dec_bit_stream_t structure.
 *
 * \return
 *    None.
 *
 **************************************************************************
 */
void ih264d_rbsp_to_sodb(dec_bit_stream_t *ps_bitstrm)
{
    UWORD32 ui_lastWord;
    UWORD32 ui_word;
    UWORD8 uc_lastByte;
    WORD8 i;

    ui_lastWord = (ps_bitstrm->u4_max_ofst >> 5);
    i = (ps_bitstrm->u4_max_ofst >> 3) & 0x03;

    if(i)
    {
        ui_word = ps_bitstrm->pu4_buffer[ui_lastWord];
        uc_lastByte = ((ui_word << ((i - 1) << 3)) >> 24);
    }
    else
    {
        ui_word = ps_bitstrm->pu4_buffer[ui_lastWord - 1];
        uc_lastByte = ((ui_word << 24) >> 24);
    }
    /*--------------------------------------------------------------------*/
    /* Find out the rbsp_stop_bit position in the last byte of rbsp       */
    /*--------------------------------------------------------------------*/
    for(i = 0; (i < 8) && !CHECKBIT(uc_lastByte, i); ++i)
        ;
    ps_bitstrm->u4_max_ofst = ps_bitstrm->u4_max_ofst - (i + 1);
}
