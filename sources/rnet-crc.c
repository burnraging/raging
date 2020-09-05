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

//! @file     rnet-crc.c
//! @authors  Bernie Woodland
//! @date     7Apr18
//!
//! @brief    Checksums for RNET stack
//!
//! @details  References:
//! @details    RFC 1662: PPP in HDLC-like Framing
//! @details    RFC 1661: The Point-to-Point Protocol
//!

#include "rnet-crc.h"

// Size of stack temporary object
// Larger value= less CPU time; Smaller value= less stack RAM
#define  TEMP_BUFFER_SIZE   40

//!
//! @name      rnet_crc16_buf
//!
//! @brief     Do a CRC16 over a frame
//!
//! @param[in] 'buf'-- RNET buffer object
//! @param[in]     assume frame starts at buf->header.offset
//! @param[in]     assume frame is buf->header.length bytes long
//! @param[in] 'include_final_eor'-- if 'true', calculate entire
//! @param[in]     CRC as for tx. If 'false', calculate for rx,
//! @param[in]     which assumes we're running over existing CRC.
//!
//! @return    CRC
//!
uint16_t rnet_crc16_buf(rnet_buf_t *buf, bool include_final_eor)
{
    uint8_t *frame_ptr;
    uint16_t crc;
    unsigned frame_length;

    frame_ptr = RNET_BUF_FRAME_START_PTR(buf);

    frame_length = buf->header.length;

    crc = rutils_crc16_buffer(frame_ptr, frame_length);

    if (include_final_eor)
    {
        crc = crc ^ 0xFFFF;
    }

    return crc;
}

//!
//! @name      rnet_crc16_pcl
//!
//! @brief     Do a CRC16 over a frame
//!
//! @param[in] 'head_pcl'-- RNET buffer object
//! @param[in]     assume frame starts at head_pcl->header.offset
//! @param[in]     assume frame is head_pcl->header.total_used_length long
//! @param[in] 'exclude_crc_in_frame'-- Frame as described in
//! @param[in]     'buf' has a CRC already: don't include it in
//! @param[in]     calculation.
//!
//! @return    CRC
//!
uint16_t rnet_crc16_pcl(nsvc_pcl_t *head_pcl, bool include_final_eor)
{
    nsvc_pcl_header_t *header;
    nsvc_pcl_chain_seek_t read_posit;
    uint8_t  temp_buffer[TEMP_BUFFER_SIZE];
    uint16_t crc;
    bool     rv;
    unsigned frame_length;
    unsigned chunk_length;

    header = NSVC_PCL_HEADER(head_pcl);
    frame_length = header->total_used_length;

    // Seek to beginning of frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_posit,
                                                header->offset);
    if (!rv)
    {
        // Must be an ill-formed frame
        return 0;
    }

    crc = rutils_crc16_start();

    // Step through frame. Copy frame into chunks, CRC each
    // chunk.
    while (frame_length > 0)
    {
        // 'chunk_length' is smaller of 'temp_buffer' size of
        // remaining bytes in frame
        chunk_length = TEMP_BUFFER_SIZE;
        if (chunk_length > frame_length)
        {
            chunk_length = frame_length;
        }

        if (chunk_length != nsvc_pcl_read(&read_posit,
                                          temp_buffer,
                                          chunk_length))
        {
            return 0;
        }

        crc = rutils_crc16_add_string(crc, temp_buffer, chunk_length);

        frame_length -= chunk_length;
    }

    if (include_final_eor)
    {
        crc = crc ^ 0xFFFF;
    }

    return crc;
}