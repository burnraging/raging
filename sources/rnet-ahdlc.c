/*
Copyright (c) 2018, Bernie Woodland
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//! @file     rnet-ahdlc.c
//! @authors  Bernie Woodland
//! @date     5Apr18
//!
//! @brief    HDLC/AHDCL protocol for PPP and whoeever else may want to use it
//!
//! @details  References:
//! @details    RFC 1662: PPP in HDLC-like Framing
//! @details    High-Level Data Link Control, Wikipedia,
//! @details        https://en.wikipedia.org/wiki/High-Level_Data_Link_Control
//! @details    RFC 1661: The Point-to-Point Protocol
//!

#include "raging-global.h"

#include "rnet-ahdlc.h"
#include "rnet-dispatch.h"
#include "rnet-intfc.h"
#include "rnet-crc.h"

#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-contract.h"


// Size of stack temporary object
// Larger value= less CPU time; Smaller value= less stack RAM
#define  TEMP_BUFFER_SIZE   40

//!
//! @name      rnet_ahdlc_strip_delimiters_buf
//!
//! @brief     Remove flag sequences from frame. This will adjust offset
//! @brief     and length to remove these chars, but not shift data.
//!
//! @param[in] 'buf'-- packet to have delimiters stripped
//!
void rnet_ahdlc_strip_delimiters_buf(rnet_buf_t *buf)
{
    uint8_t *ptr;
    uint8_t  character;
    bool     stripping;

    // point to beginning of frame
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    stripping = true;
    do
    {
        character = *ptr;
        
        if (RNET_AHDLC_FLAG_SEQUENCE == character)
        {
            buf->header.offset++;
            buf->header.length--;

            *ptr = 0;
        }
        else
        {
            stripping = false;
        }

        ptr++;

    } while (stripping);

    // point to end of frame (points to char after last
    ptr = RNET_BUF_FRAME_END_PTR(buf);

    stripping = true;
    do
    {
        --ptr;

        character = *ptr;
        
        if (RNET_AHDLC_FLAG_SEQUENCE == character)
        {
            buf->header.length--;

            *ptr = 0;
        }
        else
        {
            stripping = false;
        }

    } while (stripping);
}

//!
//! @name      rnet_ahdlc_strip_delimiters_pcl
//!
//! @brief     Remove flag sequences from frame. This will adjust offset
//! @brief     and length to remove these chars, but not shift data.
//!
//! @param[in] 'head_pcl'-- packet to have delimiters stripped
//!
void rnet_ahdlc_strip_delimiters_pcl(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t     *header;
    nsvc_pcl_chain_seek_t  read_write_posit;
    uint8_t                character;
    bool                   stripping;
    bool                   rv;

    // point to beginning of frame
    header = NSVC_PCL_HEADER(head_pcl);

    if (header->total_used_length < 2*AHDLC_FLAG_CHAR_SIZE)
    {
        // shortcut, fixme
        SL_REQUIRE(0);
        return;
    }

    // Begin walk from start of frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_write_posit,
                                                header->offset);
    if (!rv)
    {
        SL_REQUIRE(0);
        return;
    }


    stripping = true;
    do
    {
        // Read single byte
        rv = nsvc_pcl_read(&read_write_posit,
                           &character,
                           AHDLC_FLAG_CHAR_SIZE);
        if (!rv)
        {
            SL_REQUIRE(0);
            return;
        }

        // Is it a flag sequence        
        if (RNET_AHDLC_FLAG_SEQUENCE == character)
        {
            header->offset++;
            header->total_used_length--;

            // Reset back onto that byte
            rv = nsvc_pcl_seek_rewind(head_pcl,
                                      &read_write_posit,
                                      AHDLC_FLAG_CHAR_SIZE);
            if (!rv)
            {
                SL_REQUIRE(0);
                return;
            }

            // Clear out byte
            character = 0;

            // Write it
            rv = nsvc_pcl_write_data_continue(&read_write_posit,
                                              &character,
                                              AHDLC_FLAG_CHAR_SIZE);
            if (!rv)
            {
                SL_REQUIRE(0);
                return;
            }
        }
        else
        {
            stripping = false;
        }

    } while (stripping);

    // Position to last character in frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_write_posit,
                                header->offset + header->total_used_length - 1);
    if (!rv)
    {
        SL_REQUIRE(0);
        return;
    }


    stripping = true;
    do
    {
        // Read single byte
        rv = nsvc_pcl_read(&read_write_posit, &character,
                           AHDLC_FLAG_CHAR_SIZE);
        if (!rv)
        {
            SL_REQUIRE(0);
            return;
        }

        // Is it a flag sequence        
        if (RNET_AHDLC_FLAG_SEQUENCE == character)
        {
            header->total_used_length--;

            // Reset back onto that byte
            rv = nsvc_pcl_seek_rewind(head_pcl, &read_write_posit,
                                      AHDLC_FLAG_CHAR_SIZE);
            if (!rv)
            {
                SL_REQUIRE(0);
                return;
            }

            // Clear out byte
            character = 0;

            // Write it
            rv = nsvc_pcl_write_data_continue(&read_write_posit,
                                    &character, AHDLC_FLAG_CHAR_SIZE);
            if (!rv)
            {
                SL_REQUIRE(0);
                return;
            }
        }
        else
        {
            stripping = false;
        }

    } while (stripping);
}

//!
//! @name      rnet_ahdlc_encode_delimiters_buf
//!
//! @brief     Add single pair of start/end delimiters to frame.
//!
//! @param[in] 'buf'-- packet to have delimiters stripped
//!
void rnet_ahdlc_encode_delimiters_buf(rnet_buf_t *buf)
{
    uint8_t *ptr;

    // Sanity check meta data in header
    //  Enough room to prepend?
    if (buf->header.offset < AHDLC_FLAG_CHAR_SIZE)
    {
        SL_REQUIRE(0);
        return;
    }
    // Enough room to append?
    if (buf->header.offset + buf->header.length >= RNET_BUF_SIZE)
    {
        SL_REQUIRE(0);
        return;
    }

    buf->header.offset -= AHDLC_FLAG_CHAR_SIZE;
    buf->header.length += AHDLC_FLAG_CHAR_SIZE;

    // point to beginning of frame
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    *ptr = RNET_AHDLC_FLAG_SEQUENCE;

    // point to end of frame (points to char after last
    ptr = RNET_BUF_FRAME_END_PTR(buf);

    *ptr = RNET_AHDLC_FLAG_SEQUENCE;

    buf->header.length += AHDLC_FLAG_CHAR_SIZE;
}

//!
//! @name      rnet_ahdlc_encode_delimiters_pcl
//!
//! @brief     Add single pair of start/end delimiters to frame.
//!
//! @param[in] 'head_pcl'-- packet to have delimiters stripped
//!
void rnet_ahdlc_encode_delimiters_pcl(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t     *header;
    nsvc_pcl_chain_seek_t  read_write_posit;
    uint8_t                character = RNET_AHDLC_FLAG_SEQUENCE;
    bool                   rv;

    // point to beginning of frame
    header = NSVC_PCL_HEADER(head_pcl);

    // Sanity check meta data in header
    if (header->offset < NSVC_PCL_OFFSET_PAST_HEADER(AHDLC_FLAG_CHAR_SIZE))
    {
        // shortcut, fixme
        SL_REQUIRE(0);
        return;
    }

    // Position to char before start of frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                            &read_write_posit,
                               header->offset - AHDLC_FLAG_CHAR_SIZE);
    if (!rv)
    {
        SL_REQUIRE(0);
        return;
    }

    // Write delimiter
    rv = nsvc_pcl_write_data_continue(&read_write_posit,
                                      &character,
                                      AHDLC_FLAG_CHAR_SIZE);
    if (!rv)
    {
        SL_REQUIRE(0);
        return;
    }

    header->offset -= AHDLC_FLAG_CHAR_SIZE;
    header->total_used_length += AHDLC_FLAG_CHAR_SIZE;


    // Position to last character in frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_write_posit,
            header->offset + header->total_used_length);
    if (!rv)
    {
        SL_REQUIRE(0);
        return;
    }

    // Write delimiter
    rv = nsvc_pcl_write_data_continue(&read_write_posit,
                                      &character,
                                      AHDLC_FLAG_CHAR_SIZE);
    if (!rv)
    {
        SL_REQUIRE(0);
        return;
    }

    header->total_used_length += AHDLC_FLAG_CHAR_SIZE;
}

//!
//! @name      rnet_ahdlc_strip_control_chars_linear
//!
//! @brief     Given a frame with escape characters and possibly with
//! @brief     extra leading/trailing flag sequences, remove AHDLC formatting.
//!
//! @param[in] 'buffer'-- contiguous ram buffer /w frame starting
//! @param[in]            at 'buffer[0]'
//! @param[out] 'buffer'-- translated result put back in input, starting
//! @param[out]         at beginning.
//! @param[in] 'length'-- frame size/used length in 'buffer'
//! @param[in] 'data_will_continue'-- buffer is not end of data stream;
//! @param[in]        If you reach end and need to translate, peek to next char
//!
//! @return   Length of stripped buffer; RNET_AHDLC_FORMATTING_ERROR upon error.
//!
int rnet_ahdlc_strip_control_chars_linear(uint8_t  *buffer,
                                          unsigned  length,
                                          bool      data_will_continue)
{
    uint8_t *start_ptr = buffer;
    uint8_t *end_ptr = start_ptr + length;
    uint8_t *src_ptr;
    uint8_t *dest_ptr;
    uint8_t character;
    uint8_t next_character;
    uint8_t computed_character;
    int     stripped_length;

    src_ptr = start_ptr;
    dest_ptr = start_ptr;

    while (src_ptr < end_ptr)
    {
        character = *src_ptr++;

        if (RNET_AHDLC_CONTROL_ESCAPE == character)
        {
            // Sanity check to ensure we're not overrunning buffer
            if (!data_will_continue && (src_ptr == end_ptr))
            {
                return RNET_AHDLC_FORMATTING_ERROR;
            }

            next_character = *src_ptr++;
            computed_character = next_character ^ RNET_AHDLC_MAGIC_EOR;
        }
        // If an unescaped flag sequence is encountered, it must be
        // at the end of the frame, extra frame-delimiting flag sequences.
        // Verify that that is the case;
        //   consume them to end of frame otherwise.
        else if (RNET_AHDLC_FLAG_SEQUENCE == character)
        {
            return RNET_AHDLC_FORMATTING_ERROR;
        }
        else
        {
            computed_character = character;
        }

        // Write escaped or non-escaped value back into buffer
        *dest_ptr++ = computed_character;
    }

    stripped_length = (int)(dest_ptr - start_ptr);

    return stripped_length;
}

//!
//! @name      rnet_ahdlc_strip_control_chars_buf
//!
//! @brief     Given a frame with escape characters and possibly with
//! @brief     extra leading/trailing flag sequences, remove AHDLC formatting.
//!
//! @param[in] 'buf'-- RNET linear buffer type
//! @param[out] 'buf'-- translated result put back in input, starting
//! @param[out]         at beginning. buf->header.length is adjusted too.
//!
//! @return   'true' if no error
//!
bool rnet_ahdlc_strip_control_chars_buf(rnet_buf_t *buf)
{
    uint8_t *buf_start_ptr;
    int      adjusted_length;

    // 'buf_start_ptr' must point to beginning of frame
    buf_start_ptr = RNET_BUF_FRAME_START_PTR(buf);

    adjusted_length = rnet_ahdlc_strip_control_chars_linear(buf_start_ptr,
                                                            buf->header.length,
                                                            false);

    if (adjusted_length < 0)
    {
        return false;
    }

    // Adjust header length
    buf->header.length = (unsigned)adjusted_length;

    return true;
}

//!
//! @name      rnet_ahdlc_strip_control_chars_pcl
//!
//! @brief     Given a particle chain that contains an AHDLC frame,
//! @brief     strip the AHDLC control characters from it.
//!
//! @details   Same algorithm as 'rnet_ahdlc_strip_control_chars_pcl()'
//! @details   Assumes chain header is formatted with sane offset and
//! @details   total used length values.
//! @details   Output is put back in chain at same start offset.
//!
//! @param[in] 'head_pcl'-- particle chain
//! @param[out] 'head_pcl'-- particle chain: output put back on same chain
//! @param[out]         at beginning.
//!
//! @return   'true' if no errors
//!
bool rnet_ahdlc_strip_control_chars_pcl(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t *header;
    bool rv;
    const unsigned MIN_FRAME_SIZE = 4;
    nsvc_pcl_chain_seek_t read_posit;
    nsvc_pcl_chain_seek_t write_posit;
    uint8_t  temp_buffer[TEMP_BUFFER_SIZE];
    bool     has_more_data;
    unsigned frame_length;
    unsigned frame_index;
    unsigned selected_length;
    unsigned read_length;
    unsigned stripped_length;
    int      int_stripped_length;
    unsigned total_stripped_length = 0;

    header = NSVC_PCL_HEADER(head_pcl);
    frame_length = header->total_used_length;

    // Sanity check
    if (frame_length < MIN_FRAME_SIZE)
    {
        return false;
    }

    // Set 'read_posit', 'write_posit' to beginning of frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_posit,
                                                header->offset);
    rutils_memcpy(&write_posit, &read_posit, sizeof(write_posit));
    if (!rv)
    {
        SL_REQUIRE(0);
        return false;
    }

    // Purge control characters
    // Step through frame in 'temp_buffer' blocks
    // Read position and write positions move forward, with
    // read position moving same/faster than write position.
    // In this way, writes never overwrite unread data.
    frame_index = frame_length;
    while (frame_index > 0)
    {
        if (!rv)
        {
            SL_REQUIRE(0);
            return false;
        }

        // Select greater of 'temp_buffer' size or bytes remaining in pcl
        // Select 1 less than buffer capacity, as read will tack on 1
        // extra byte.
        if (frame_index <= TEMP_BUFFER_SIZE)
        {
            has_more_data = false;
            selected_length = frame_index;
        }
        else
        {
            has_more_data = true;
            selected_length = TEMP_BUFFER_SIZE - 1;
        }

        // fill up 'temp_buffer'
        // 1st case: not the final buffer
        if (has_more_data)
        {
            read_length = nsvc_pcl_read(&read_posit,
                                        temp_buffer,
                                        selected_length + 1);

            // Was 2nd to last byte a translation escape sequence? Then keep
            // read was ok. Otherwise, rewind and 1 byte for next read
            // This ensures that we handle escape sequences which start on one
            // 'temp_buffer' and continue onto the next.
            if (RNET_AHDLC_CONTROL_ESCAPE != temp_buffer[selected_length - 1])
            {
                // rewind 'read_posit' for that 1 extra byte
                nsvc_pcl_seek_rewind(head_pcl, &read_posit, 1);

                // Adjust 'read_length', to reflect the fact that the last
                // byte in 'temp_buffer' was ignored (since the 2nd to last
                // wasn't an escape char), and will have to be processed
                // by next read.
                read_length--;
            }
        }
        // This is the final 'temp_buffer' read: end of frame is reached
        else
        {
            read_length = nsvc_pcl_read(&read_posit,
                                        temp_buffer,
                                        selected_length);
        }

        // Did read complete ok?
        if (read_length < selected_length)
        {
            SL_REQUIRE(0);
            return false;
        }

        // Strip data in 'temp_buffer'. This will shorten 'temp_buffer'
        // to be 'int_stripped_length' size.
        int_stripped_length =
              rnet_ahdlc_strip_control_chars_linear(temp_buffer,
                                                    read_length,
                                                    has_more_data);

        // Op completed ok?
        if (int_stripped_length < 0)
        {
            SL_REQUIRE(0);
            return false;
        }
        stripped_length = (unsigned)int_stripped_length;

        // Sanity check:
        // 'stripped_length' must always be <= 'read_length'
        if (stripped_length > read_length)
        {
            SL_REQUIRE(0);
            return false;
        }

        // Write stripped data back onto pcl. Since it's same or shorter
        // than what was read, this shouldn't overwrite yet unread data.
        if (stripped_length != nsvc_pcl_write_data_continue(&write_posit,
                                                            temp_buffer,
                                                            stripped_length))
        {
            SL_REQUIRE(0);
            return false;
        }

        frame_index -= read_length;
        total_stripped_length += stripped_length;
    }

    // Adjust frame size to reflect loss of control chars
    header->total_used_length = total_stripped_length;

    return true;
}

//!
//! @name      rnet_ahdlc_encode_control_chars_dual
//!
//! @brief     Insert AHDLC control characters into a destination buffer
//! @brief     based on a frame in a source buffer.
//!
//! @details   This fcn does not add leading or trailing flag sequences
//! @details   (frame delimiters). Therefore, this fcn can be used
//! @details   to do partial frame encodings.
//!
//! @param[in] 'src_buffer'-- frame of length 'src_buffer_length' to
//! @param[in]       add control chars to. Frame must start at 'src_buffer[0]'
//! @param[in] 'src_buffer_length'-- length of src frame
//! @param[out] 'dest_buffer'-- frame with control characters
//! @param[in]  'dest_buffer_length'-- max size of 'dest_buffer'
//! @param[out] 'dest_buffer_length'-- frame size as appears in 'dest_buffer'
//!
//! @return   'false' if destination buffer overrun occurred
//!
bool rnet_ahdlc_encode_control_chars_dual(const uint8_t *src_buffer,
                                          unsigned       src_buffer_length,
                                          uint8_t       *dest_buffer,
                                          unsigned      *dest_buffer_length)
{
    unsigned dest_buffer_size = *dest_buffer_length;
    unsigned output_length = 0;
    unsigned i;
    uint8_t  character;

    for (i = 0; i < src_buffer_length; i++)
    {
        character = src_buffer[i];

        // Since this is only used for "asynchronous framing" and never
        // "synchronous framing", there's no "bit stuffing" done.
        // Bit stuffing translates characters values < RNET_AHDLC_MAGIC_EOR, and
        // not just translations of RNET_AHDLC_FLAG_SEQUENCE and
        //  RNET_AHDLC_CONTROL_ESCAPE,
        // adding many more translations.
        if ((RNET_AHDLC_FLAG_SEQUENCE == character) ||
            (RNET_AHDLC_CONTROL_ESCAPE == character))
        {
            // Sanity check for output buffer overrun
            if ((output_length + 1) >= dest_buffer_size)
            {
                return false;
            }

            dest_buffer[output_length++] = RNET_AHDLC_CONTROL_ESCAPE;
            dest_buffer[output_length++] = character ^ RNET_AHDLC_MAGIC_EOR;
        }
        // No translation necessary
        else
        {
            if (output_length >= dest_buffer_size)
            {
                return false;
            }

            dest_buffer[output_length++] = character;
        }
    }

    *dest_buffer_length = output_length;

    return true;
}

//!
//! @name      rnet_ahdlc_encode_control_chars_buf
//!
//! @brief     Insert AHDLC control characters into an RNET buffer.
//!
//! @details   Readjust RNET buffer length 
//! @details   (frame delimiters). Therefore, this fcn can be used
//! @details   to do partial frame encodings.
//!
//! @param[in] 'buf'-- RNET linear buffer type
//! @param[out] 'buf'-- translated result put back in input, starting
//! @param[out]         at beginning. buf->header.length is adjusted too.
//!
//! @return   'false' if buffer overrun occurred
//!
bool rnet_ahdlc_encode_control_chars_buf(rnet_buf_t *buf,
                                         unsigned    translation_count)
{
    unsigned i;
    unsigned src_frame_length = buf->header.length;
    uint8_t *src_ptr;
    uint8_t *dest_ptr;
    uint8_t character;
    unsigned new_extent;

    // Need to encode?
    if (0 == translation_count)
    {
        return true;
    }

    src_ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Calculate extent that encoded frame will consume bytes in the buffer.
    // Translated frame will begin at same starting offset.
    new_extent = buf->header.offset + buf->header.length + translation_count;

    // Sanity check buf sizing: must be big enough to hold extra chars
    if (new_extent > RNET_BUF_SIZE)
    {
        return false;
    }

    // 'src_ptr' points to end of current frame (+1 past last byte);
    // 'dest_ptr' points to end (+1) of where translated frame will be
    src_ptr = RNET_BUF_FRAME_END_PTR(buf);
    dest_ptr = src_ptr + translation_count;

    // Walk backward through source frame so that destination writes
    // won't overwrite any untranslated bytes in source frame.
    for (i = 0; i < src_frame_length; i++)
    {
        character = *(--src_ptr);

        if ((RNET_AHDLC_FLAG_SEQUENCE == character) ||
            (RNET_AHDLC_CONTROL_ESCAPE == character))
        {
            *(--dest_ptr) = character ^ RNET_AHDLC_MAGIC_EOR;
            *(--dest_ptr) = RNET_AHDLC_CONTROL_ESCAPE;
        }
        else
        {
            *(--dest_ptr) = character;
        }
    }

    // Adjust buf length for translated characters
    buf->header.length += translation_count;

    return true;
}

//!
//! @name      rnet_ahdlc_encode_control_chars_pcl
//!
//! @brief     Given a particle chain that contains an AHDLC frame,
//! @brief     count the number of control characters needed by encoding.
//!
//! @param[in] 'head_pcl'-- particle chain. Assume
//! @param[in]      Assume chain starts at header 'offset' and ends at
//! @param[in]      header 'total_used_length'
//! @param[in] 'translation_count'-- number of additional chars that
//! @param[in]      encoding will consume
//!
//! @return   'false' if error
//!
bool rnet_ahdlc_encode_control_chars_pcl(nsvc_pcl_t *head_pcl,
                                         unsigned    translation_count)
{
    nsvc_pcl_header_t *header;
    nsvc_pcl_chain_seek_t read_posit;
    nsvc_pcl_chain_seek_t write_posit;
    uint8_t temp_read_buffer[TEMP_BUFFER_SIZE/2];
    uint8_t temp_translated_buffer[TEMP_BUFFER_SIZE];
    unsigned frame_length;
    unsigned read_length;
    unsigned bytes_read;
    unsigned bytes_written;
    unsigned expanded_length;
    unsigned total_bytes_processed = 0;
    bool     rv;

    // Need to encode?
    if (0 == translation_count)
    {
        return true;
    }

    header = NSVC_PCL_HEADER(head_pcl);
    frame_length = header->total_used_length;

    // Seek to 1 byte past frame
    // (NOTE: corner-case where this could go 1 byte past
    //   end of chain)
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_posit,
                                    header->offset + header->total_used_length);
    if (!rv)
    {
        SL_REQUIRE(0);
        return false;
    }

    // Set write position to same as read postion (last byte in frame),
    // then advance it by the number of bytes that will be translated.
    // Final write position points to what will be last byte of translated
    // frame.
    rutils_memcpy(&write_posit, &read_posit, sizeof(write_posit));
    rv = nsvc_pcl_seek_ffwd(&write_posit, translation_count);
    if (!rv)
    {
        SL_REQUIRE(0);
        return false;
    }

    // Process frame in reverse.
    // Done in reverse with writing starting behind read.
    // In this way, no writes will clobber unread frame data.
    while (frame_length > 0)
    {
        // Calculate read length
        if (frame_length <= sizeof(temp_read_buffer))
        {
            read_length = frame_length;
        }
        else
        {
            read_length = sizeof(temp_read_buffer);
        }

        // Go backwards that amount, to start of where to read from
        rv = nsvc_pcl_seek_rewind(head_pcl, &read_posit, read_length);
        if (!rv)
        {
            SL_REQUIRE(0);
            return false;
        }

        // Fill up read buffer with data
        bytes_read = nsvc_pcl_read(&read_posit, temp_read_buffer, read_length);
        if (bytes_read != read_length)
        {
            SL_REQUIRE(0);
            return false;
        }

        // Cancel seek auto-advance
        nsvc_pcl_seek_rewind(head_pcl, &read_posit, read_length);
        if (!rv)
        {
            SL_REQUIRE(0);
            return false;
        }

        // Translate chars. Put them in 'temp_write_buffer'
        expanded_length = sizeof(temp_translated_buffer);
        rv = rnet_ahdlc_encode_control_chars_dual(temp_read_buffer,
                                                  read_length,
                                                  temp_translated_buffer,
                                                  &expanded_length);
        if (!rv)
        {
            SL_REQUIRE(0);
            return false;
        }

        // We now know how far write will be, in order to rewind.
        rv = nsvc_pcl_seek_rewind(head_pcl, &write_posit, expanded_length);
        if (!rv)
        {
            SL_REQUIRE(0);
            return false;
        }

        // Write translated characters
        bytes_written = nsvc_pcl_write_data_continue(&write_posit,
                                                     temp_translated_buffer,
                                                     expanded_length);
        if (bytes_written != expanded_length)        
        {
            SL_REQUIRE(0);
            return false;
        }

        // Cancel seek auto-advance
        rv = nsvc_pcl_seek_rewind(head_pcl, &write_posit, expanded_length);
        if (!rv)
        {
            SL_REQUIRE(0);
            return false;
        }

        frame_length -= read_length;
        total_bytes_processed += expanded_length;
    }

    // Adjust frame length for control chars added.
    // Offset remains where it was.
    header->total_used_length = total_bytes_processed;

    return true;
}

//!
//! @name      rnet_ahdlc_translation_count_pcl
//!
//! @brief     Given a buffer,
//! @brief     count the number of control characters needed by encoding.
//!
//! @param[in] 'buffer'-- particle chain. Assume
//! @param[in] 'length'-- data in buffer
//!
//! @return   Number of additional characters needed by translation
//!
unsigned rnet_ahdlc_translation_count_linear(uint8_t *buffer,
                                             unsigned length)
{
    unsigned translation_count = 0;
    uint8_t character;

    while (length > 0)
    {
        character = *buffer++;

        if ((RNET_AHDLC_FLAG_SEQUENCE == character) ||
            (RNET_AHDLC_CONTROL_ESCAPE == character))
        {
            translation_count++;
        }

        length--;
    }

    return translation_count;
}

//!
//! @name      rnet_ahdlc_translation_count_pcl
//!
//! @brief     Given a particle chain that contains an AHDLC frame,
//! @brief     count the number of control characters needed by encoding.
//!
//! @param[in] 'head_pcl'-- particle chain. Assume
//! @param[in]      Assume chain starts at header 'offset' and ends at
//! @param[in]      header 'total_used_length'
//!
//! @return   Number of additional characters needed by translation
//!
unsigned rnet_ahdlc_translation_count_pcl(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t *header;
    unsigned frame_length;
    uint8_t *read_pcl_ptr;
    nsvc_pcl_t *read_pcl;
    unsigned read_pcl_length;
    unsigned this_length;
    nsvc_pcl_chain_seek_t read_posit;
    unsigned translation_count = 0;

    header = NSVC_PCL_HEADER(head_pcl);
    frame_length = header->total_used_length;

    // Set read seek to beginning of frame
    (void)nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                 &read_posit,
                                                 header->offset);

    // Use seek info to initialize pcl info
    read_pcl_length = NSVC_PCL_SIZE - read_posit.offset_in_pcl;
    read_pcl = read_posit.current_pcl;
    read_pcl_ptr = &read_pcl->buffer[read_posit.offset_in_pcl];

    // Loop through as many bytes as are in the frame
    while (frame_length > 0)
    {
        // Refresh read pcl?
        if (0 == read_pcl_length)
        {
            read_pcl = read_pcl->flink;
            if (NULL == read_pcl)
            {
                // Should never get here
                return 0;
            }

            read_pcl_length = NSVC_PCL_SIZE;
            read_pcl_ptr = read_pcl->buffer;
        }

        this_length = read_pcl_length;
        if (this_length > frame_length)
        {
            this_length = frame_length;
        }

        // Step through all the bytes in this pcl
        translation_count += rnet_ahdlc_translation_count_linear(read_pcl_ptr,
                                                                 this_length);

        frame_length -= this_length;
        read_pcl_length = 0; 
    }

    return translation_count;
}

//!
//! @name      rnet_msg_rx_buf_ahdlc_strip_cc
//!
//! @brief     Top-level API from RNET stack for processing an AHDLC frame
//! @brief     which requires decoding of AHDLC control chars.
//!
//! @param[in] 'buf'-- RNET linear buffer type
//!
void rnet_msg_rx_buf_ahdlc_strip_cc(rnet_buf_t *buf)
{
    bool rv;

    SL_REQUIRE(IS_RNET_BUF(buf));

    rnet_ahdlc_strip_delimiters_buf(buf);

    rv = rnet_ahdlc_strip_control_chars_buf(buf);

    if (!rv)
    {
        buf->header.code = RNET_BUF_CODE_AHDLC_RX_CC;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
    else
    {
        rnet_msg_send(RNET_ID_RX_BUF_AHDLC_VERIFY_CRC, buf);
    }
}

//!
//! @name      rnet_msg_rx_pcl_ahdlc_strip_cc
//!
//! @brief     Top-level API from RNET stack for processing an AHDLC frame
//! @brief     which requires decoding of AHDLC control chars.
//!
//! @param[in] 'head_pcl'-- particle chain containing frame
//!
void rnet_msg_rx_pcl_ahdlc_strip_cc(nsvc_pcl_t *head_pcl)
{
    bool               rv;
    nsvc_pcl_header_t *header;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    header = NSVC_PCL_HEADER(head_pcl);

    rnet_ahdlc_strip_delimiters_pcl(head_pcl);

    rv = rnet_ahdlc_strip_control_chars_pcl(head_pcl);

    if (!rv)
    {
        header->code = RNET_BUF_CODE_AHDLC_RX_CC;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
    else
    {
        rnet_msg_send(RNET_ID_RX_PCL_AHDLC_VERIFY_CRC, head_pcl);
    }
}

//!
//! @name      rnet_msg_rx_buf_ahdlc_verify_crc
//!
//! @brief     API from RNET stack for verifying+stripping an AHDLC CRC16
//!
//! @param[in] 'buf'-- RNET linear buffer type
//!
void rnet_msg_rx_buf_ahdlc_verify_crc(rnet_buf_t *buf)
{
    rnet_l2_t intfc_l2_type;
    uint16_t calculated_crc;
//    uint16_t crc_in_frame;
    const rnet_intfc_rom_t *rom_intfc_ptr;

    SL_REQUIRE(IS_RNET_BUF(buf));

//   This code works, but demonstrating how CRCs are calculated
//    calculated_crc = rnet_crc16_buf(buf, true);
//
//    ptr = RNET_BUF_FRAME_END_PTR(buf);
//    crc_in_frame = *(--ptr) << BITS_PER_WORD8;  // CRC is little-endian
//    crc_in_frame |= *(--ptr);
//
//    if (crc_in_frame != calculated_crc)
//    {
//        buf->header.code = RNET_BUF_CODE_BAD_CRC;
//        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
//    }
    calculated_crc = rnet_crc16_buf(buf, false);

    if (RUTILS_CRC16_GOOD != calculated_crc)
    {
        buf->header.code = RNET_BUF_CODE_AHDLC_RX_BAD_CRC;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
    else
    {
        // Remove CRC from frame
        buf->header.length -= RUTILS_CRC16_SIZE;

        // Which protocol were we processing? Do next action based on it.
        rom_intfc_ptr = rnet_intfc_get_rom((rnet_intfc_t)buf->header.intfc);
        if (NULL != rom_intfc_ptr)
        {
            intfc_l2_type = rom_intfc_ptr->l2_type;

            if (RNET_L2_PPP == intfc_l2_type)
            {
                rnet_msg_send(RNET_ID_RX_BUF_PPP, buf);
            }
            else
            {
                buf->header.code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
                rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
            }
        }
    }
}

//!
//! @name      rnet_msg_rx_pcl_ahdlc_verify_crc
//!
//! @brief     API from RNET stack for verifying+stripping an AHDLC CRC16
//!
//! @param[in] 'head_pcl'-- particle chain containing frame
//!
void rnet_msg_rx_pcl_ahdlc_verify_crc(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t    *header;
    uint16_t              calculated_crc;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    calculated_crc = rnet_crc16_pcl(head_pcl, false);

    header = NSVC_PCL_HEADER(head_pcl);

    if (RUTILS_CRC16_GOOD != calculated_crc)
    {
        header->code = RNET_BUF_CODE_AHDLC_RX_BAD_CRC;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
    else
    {
        // Remove CRC from frame
        header->total_used_length -= RUTILS_CRC16_SIZE;

        // Which protocol were we processing? Do next action based on it.
        if (RNET_L2_PPP == rnet_intfc_get_type((rnet_intfc_t)header->intfc))
        {
            rnet_msg_send(RNET_ID_RX_PCL_PPP, head_pcl);
        }
        else
        {
            header->code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }
    }
}

//!
//! @name      rnet_msg_tx_buf_ahdlc_crc
//!
//! @brief     API from RNET stack for appending AHDLC's CRC16 to frame
//!
//! @param[in] 'buf'-- RNET linear buffer type
//!
void rnet_msg_tx_buf_ahdlc_crc(rnet_buf_t *buf)
{
    uint8_t     *ptr;
    uint16_t     calculated_crc;
    rnet_intfc_t intfc;
    unsigned     options;

    SL_REQUIRE(IS_RNET_BUF(buf));

    calculated_crc = rnet_crc16_buf(buf, true);

    ptr = RNET_BUF_FRAME_END_PTR(buf);

    // Discard frame if CRC would overrun end of buffer
    if (buf->header.offset + buf->header.length + RUTILS_CRC16_SIZE >
                                                          RNET_BUF_SIZE)
    {
        buf->header.code = RNET_BUF_CODE_MTU_EXCEEDED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    // CRC is little-endian
    rutils_word16_to_stream_little_endian(ptr, calculated_crc);
    ptr += RUTILS_CRC16_SIZE;

    buf->header.length += RUTILS_CRC16_SIZE;

    intfc = (rnet_intfc_t)buf->header.intfc;
    options = rnet_intfc_get_options(intfc);

    if ((options & RNET_IOPT_OMIT_TX_AHDLC_TRANSLATION) != 0)
    {
        rnet_msg_send(RNET_ID_TX_BUF_DRIVER, buf);
    }
    else
    {
        rnet_msg_send(RNET_ID_TX_BUF_AHDLC_ENCODE_CC, buf);
    }
}

//!
//! @name      rnet_msg_tx_pcl_ahdlc_crc
//!
//! @brief     API from RNET stack for appending AHDLC's CRC16 to frame
//!
//! @param[in] 'head_pcl'-- particle chain containing frame
//!
void rnet_msg_tx_pcl_ahdlc_crc(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t    *header;
    nsvc_pcl_chain_seek_t write_posit;
    bool                  rv;
    unsigned              remaining_in_pcl;
    unsigned              crc_offset;
    uint16_t              calculated_crc;
    uint8_t               crc_data[RUTILS_CRC16_SIZE];
    unsigned              write_length;
    rnet_intfc_t          intfc;
    unsigned              options;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    calculated_crc = rnet_crc16_pcl(head_pcl, true);

    header = NSVC_PCL_HEADER(head_pcl);
    crc_offset = header->offset + header->total_used_length;

    remaining_in_pcl = nsvc_pcl_chain_capacity(header->num_pcls, true);
    remaining_in_pcl -= header->offset - NSVC_PCL_OFFSET_PAST_HEADER(0)
                         + header->total_used_length;

    // Sanity check that CRC data won't be written beyond end of
    // last pcl in chain.
    if (RUTILS_CRC16_SIZE > remaining_in_pcl)
    {
        // Grow chain to accomodate the CRC
        // If pcl pool is empty, this call will fail
        if (NUFR_SEMA_GET_TIMEOUT == nsvc_pcl_lengthen_chainWT(
                                 head_pcl,
                                 RUTILS_CRC16_SIZE,
                                 NSVC_PCL_NO_TIMEOUT))
        {
            header->code = RNET_BUF_CODE_NO_MORE_PCLS;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }
    }

    // CRC is little-endian
    rutils_word16_to_stream_little_endian(crc_data, calculated_crc);

    // Write CRC
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &write_posit,
                                                crc_offset);
    UNUSED(rv);         //suppress warning
    write_length = nsvc_pcl_write_data_continue(&write_posit,
                                               crc_data,
                                               RUTILS_CRC16_SIZE);

    // Did write complete ok?
    if (RUTILS_CRC16_SIZE != write_length)
    {
        header->code = RNET_BUF_CODE_PCL_OP_FAILED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    header->total_used_length += RUTILS_CRC16_SIZE;

    intfc = (rnet_intfc_t)header->intfc;
    options = rnet_intfc_get_options(intfc);

    if ((options & RNET_IOPT_OMIT_TX_AHDLC_TRANSLATION) != 0)
    {
        rnet_msg_send(RNET_ID_TX_PCL_DRIVER, head_pcl);
    }
    else
    {
        rnet_msg_send(RNET_ID_TX_PCL_AHDLC_ENCODE_CC, head_pcl);
    }
}

//!
//! @name      rnet_msg_tx_buf_ahdlc_encode_cc
//!
//! @brief     API from RNET stack for adding AHDLC escape sequences
//!
//! @details   Addes frame delimiters also
//!
//! @param[in] 'buf'-- RNET linear buffer type
//!
void rnet_msg_tx_buf_ahdlc_encode_cc(rnet_buf_t *buf)
{
    uint8_t *ptr;
    bool     rv;
    unsigned translation_count;
    const unsigned NUM_DELIMITERS = 2 * AHDLC_FLAG_CHAR_SIZE;

    SL_REQUIRE(IS_RNET_BUF(buf));

    ptr = RNET_BUF_FRAME_START_PTR(buf);

    translation_count = rnet_ahdlc_translation_count_linear(ptr,
                                          buf->header.length);

    // Will the escape sequence translation cause frame to
    // exceed buffer length? If so, drop it.
    if (translation_count + buf->header.length + NUM_DELIMITERS > RNET_BUF_SIZE) 
    {
        buf->header.code = RNET_BUF_CODE_MTU_EXCEEDED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    rv = rnet_ahdlc_encode_control_chars_buf(buf,
                                             translation_count);
    rnet_ahdlc_encode_delimiters_buf(buf);

    if (rv)
    {
        rnet_msg_send(RNET_ID_TX_BUF_DRIVER, buf);
    }
    else
    {
        buf->header.code = RNET_BUF_CODE_AHDLC_TX_CC;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_tx_pcl_ahdlc_encode_cc
//!
//! @brief     API from RNET stack for adding AHDLC escape sequences
//!
//! @details   Addes frame delimiters also
//!
//! @param[in] 'head_pcl'-- particle chain containing frame
//!
void rnet_msg_tx_pcl_ahdlc_encode_cc(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t    *header;
    bool                  rv;
    const unsigned NUM_DELIMITERS = 2 * AHDLC_FLAG_CHAR_SIZE;
    unsigned              translation_count;
    unsigned              total_extra_count;
    unsigned              remaining_capacity;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    translation_count = rnet_ahdlc_translation_count_pcl(head_pcl);
    // Need to accomodate start/end of frame flag sequences too.
    total_extra_count = translation_count + NUM_DELIMITERS;

    header = NSVC_PCL_HEADER(head_pcl);

    // How many spare bytes past end of frame in current chain?
    remaining_capacity = nsvc_pcl_chain_capacity(header->num_pcls, true);
    remaining_capacity -= header->offset - NSVC_PCL_OFFSET_PAST_HEADER(0)
                         + header->total_used_length;

    // Is chain large enough to handle extra control character?
    // If not, lengthen it to accomodate entra chars.
    if (remaining_capacity < total_extra_count)
    {
        // If pcl pool is empty, this call will fail
        if (NUFR_SEMA_GET_TIMEOUT == nsvc_pcl_lengthen_chainWT(
                                 head_pcl,
                                 total_extra_count - remaining_capacity,
                                 NSVC_PCL_NO_TIMEOUT))
        {
            header->code = RNET_BUF_CODE_NO_MORE_PCLS;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }
    }

    rv = rnet_ahdlc_encode_control_chars_pcl(head_pcl,
                                             translation_count);
    rnet_ahdlc_encode_delimiters_pcl(head_pcl);

    if (rv)
    {
        rnet_msg_send(RNET_ID_TX_PCL_DRIVER, head_pcl);
    }
    else
    {
        header->code = RNET_BUF_CODE_PCL_OP_FAILED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}