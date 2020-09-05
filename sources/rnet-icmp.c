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

//! @file     rnet-icmp.c
//! @authors  Bernie Woodland
//! @date     12May18
//!
//! @brief    ICMP Headers
//!

#include "raging-global.h"

#include "rnet-ip.h"
#include "rnet-icmp.h"
#include "rnet-dispatch.h"
#include "rnet-intfc.h"

#include "raging-utils.h"
#include "raging-contract.h"

// Local functions
static void icmp_serialize_header(uint8_t            *buffer,
                                  rnet_icmp_header_t *header);
static void icmp_deserialize_header(rnet_icmp_header_t *header,
                                    uint8_t            *buffer);
static void icmpv6_serialize_header(uint8_t              *buffer,
                                    rnet_icmpv6_header_t *header);
static void icmpv6_deserialize_header(rnet_icmpv6_header_t *header,
                                      uint8_t              *buffer);

//!
//! @name      rnet_msg_rx_buf_icmp
//!
//! @brief     Entry point for ICMP packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_buf_icmp(rnet_buf_t *buf)
{
    uint8_t               *ptr;
    rnet_icmp_header_t     header;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of eader
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Sanity check that length is at least header size
    // sanity check that header offset+length don't overrrun RNET buffer
    if ((buf->header.length < ICMP_HEADER_SIZE) ||
        (buf->header.offset + buf->header.length > RNET_BUF_SIZE))
    {
        buf->header.code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    icmp_deserialize_header(&header, ptr);

    // We only support echo requests (pings)
    if (RNET_IT_ECHO_REQUEST != header.type)
    {
        rnet_msg_buf_discard(buf);
        return;
    }

    // Turn around ping reply
    header.type = RNET_IT_ECHO_REPLY;
    header.code = 0;
    header.checksum = 0;
    buf->header.previous_ph = RNET_PH_ICMP;
    buf->header.circuit = RNET_CIR_INDEX_SWAP_SRC_DEST;

    icmp_serialize_header(ptr, &header);

    rnet_msg_send(RNET_ID_TX_BUF_IPV4, buf);
}

//!
//! @name      rnet_msg_rx_pcl_icmp
//!
//! @brief     Entry point for ICMP packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_pcl_icmp(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t     *pcl_header;
    uint8_t               *ptr;
    rnet_icmp_header_t     header;
    unsigned               chain_capacity;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    // 'ptr' points to beginning of header
    pcl_header = NSVC_PCL_HEADER(head_pcl);
    ptr = &(head_pcl->buffer)[pcl_header->offset];

    // Sanity check that length is at least header size
    // sanity check that header offset+length don't overrrun chain
    chain_capacity = nsvc_pcl_chain_capacity(
                        nsvc_pcl_count_pcls_in_chain(head_pcl), true);
    if ((pcl_header->total_used_length < ICMP_HEADER_SIZE) ||
        ((unsigned)(pcl_header->offset + pcl_header->total_used_length) >
                 chain_capacity))
    {
        pcl_header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    icmp_deserialize_header(&header, ptr);

    // We only support echo requests (pings)
    if (RNET_IT_ECHO_REQUEST != header.type)
    {
        rnet_msg_pcl_discard(head_pcl);
        return;
    }

    // Turn around ping reply
    header.type = RNET_IT_ECHO_REPLY;
    header.code = 0;
    header.checksum = 0;
    pcl_header->previous_ph = RNET_PH_ICMP;
    pcl_header->circuit = RNET_CIR_INDEX_SWAP_SRC_DEST;

    icmp_serialize_header(ptr, &header);

    rnet_msg_send(RNET_ID_TX_PCL_IPV4, head_pcl);
}

//!
//! @name      rnet_msg_rx_buf_icmpv6
//!
//! @brief     Entry point for ICMPv6 packets
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_buf_icmpv6(rnet_buf_t *buf)
{
    uint8_t               *ptr;
    rnet_icmpv6_header_t   header;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of header
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Sanity check that length is at least header size
    // sanity check that header offset+length don't overrrun RNET buffer
    if ((buf->header.length < ICMPV6_HEADER_SIZE) ||
        (buf->header.offset + buf->header.length > RNET_BUF_SIZE))
    {
        buf->header.code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    icmpv6_deserialize_header(&header, ptr);

    // We only support echo requests (pings)
    if (RNET_ITV6_ECHO_REQUEST != header.type)
    {
        rnet_msg_buf_discard(buf);
        return;
    }

    // Turn around ping reply
    header.type = RNET_ITV6_ECHO_REPLY;
    header.code = 0;
    header.checksum = 0;
    buf->header.previous_ph = RNET_PH_ICMPv6;
    buf->header.circuit = RNET_CIR_INDEX_SWAP_SRC_DEST;

    icmpv6_serialize_header(ptr, &header);

    rnet_msg_send(RNET_ID_TX_BUF_IPV6, buf);
}

//!
//! @name      rnet_msg_rx_pcl_icmpv6
//!
//! @brief     Entry point for ICMPv6 packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_pcl_icmpv6(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t     *pcl_header;
    uint8_t               *ptr;
    rnet_icmpv6_header_t   header;
    unsigned               chain_capacity;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    // 'ptr' points to beginning of header
    pcl_header = NSVC_PCL_HEADER(head_pcl);
    ptr = &(head_pcl->buffer)[pcl_header->offset];

    // Sanity check that length is at least header size
    // sanity check that header offset+length don't overrrun chain
    chain_capacity = nsvc_pcl_chain_capacity(
                        nsvc_pcl_count_pcls_in_chain(head_pcl), true);
    if ((pcl_header->total_used_length < ICMPV6_HEADER_SIZE) ||
        ((unsigned)(pcl_header->offset + pcl_header->total_used_length) >
                       chain_capacity))
    {
        pcl_header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    icmpv6_deserialize_header(&header, ptr);

    // We only support echo requests (pings)
    if (RNET_ITV6_ECHO_REQUEST != header.type)
    {
        rnet_msg_pcl_discard(head_pcl);
        return;
    }

    // Turn around ping reply
    header.type = RNET_ITV6_ECHO_REPLY;
    header.code = 0;
    header.checksum = 0;
    pcl_header->previous_ph = RNET_PH_ICMPv6;
    pcl_header->circuit = RNET_CIR_INDEX_SWAP_SRC_DEST;

    icmpv6_serialize_header(ptr, &header);

    rnet_msg_send(RNET_ID_TX_PCL_IPV6, head_pcl);
}

//!
//! @name      icmp_serialize_header
//!
//! @brief     ICMP (for IPv4) header struct to byte stream
//!
//! @param[out] 'buffer'-- byte stream
//! @param[in] 'header'-- source info
//!
static void icmp_serialize_header(uint8_t            *buffer,
                                  rnet_icmp_header_t *header)
{
    *buffer++ = header->type;
    *buffer++ = header->code;
    
    rutils_word16_to_stream(buffer, header->checksum);
    buffer += BYTES_PER_WORD16;

    rutils_word16_to_stream(buffer,
                 header->rest_of_header.echo_request.identifier);
    buffer += BYTES_PER_WORD16;

    rutils_word16_to_stream(buffer,
                 header->rest_of_header.echo_request.sequence_number);
    buffer += BYTES_PER_WORD16;
}

//!
//! @name      icmp_deserialize_header
//!
//! @brief     scan ICMP header struct from byte stream
//!
//! @param[out] 'header'-- fill in
//! @param[in] 'buffer'-- scan starting here
//!
static void icmp_deserialize_header(rnet_icmp_header_t *header,
                                    uint8_t            *buffer)
{
    header->type = *buffer++;
    header->code = *buffer++;

    header->checksum = rutils_stream_to_word16(buffer);
    buffer += BYTES_PER_WORD16;

    header->rest_of_header.echo_request.identifier =
          rutils_stream_to_word16(buffer);
    buffer += BYTES_PER_WORD16;

    header->rest_of_header.echo_request.sequence_number =
          rutils_stream_to_word16(buffer);
    buffer += BYTES_PER_WORD16;
}

//!
//! @name      icmpv6_serialize_header
//!
//! @brief     ICMPv6 header struct to byte stream
//!
//! @param[out] 'buffer'-- byte stream
//! @param[in] 'header'-- source info
//!
static void icmpv6_serialize_header(uint8_t              *buffer,
                                    rnet_icmpv6_header_t *header)
{
    *buffer++ = header->type;
    *buffer++ = header->code;
    rutils_word16_to_stream(buffer, header->checksum);
}

//!
//! @name      icmpv6_deserialize_header
//!
//! @brief     scan ICMPv6 header struct from byte stream
//!
//! @param[out] 'header'-- fill in
//! @param[in] 'buffer'-- scan starting here
//!
static void icmpv6_deserialize_header(rnet_icmpv6_header_t *header,
                                      uint8_t              *buffer)
{
    header->type = *buffer++;
    header->code = *buffer++;
    header->checksum = rutils_stream_to_word16(buffer);
}