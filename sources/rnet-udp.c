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

//! @file     rnet-udp.c
//! @authors  Bernie Woodland
//! @date     25Mar18
//!
//! @brief    UDP Headers
//!

#include "rnet-ip.h"
#include "rnet-udp.h"
#include "rnet-intfc.h"
#include "rnet-dispatch.h"

#include "raging-utils.h"
#include "raging-contract.h"

// Local functions
static void udp_serialize_header(uint8_t                 *buffer,
                                 const rnet_udp_header_t *header);
static void udp_deserialize_header(rnet_udp_header_t *header,
                                   uint8_t           *buffer);


//!
//! @name      rnet_msg_rx_buf_udp
//!
//! @brief     Entry point for UDP packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_buf_udp(rnet_buf_t *buf)
{
    uint8_t               *ptr;
    uint8_t               *src_ip_addr_ptr;
    bool                   is_ipv6;
    rnet_udp_header_t      header;
    rnet_subi_t            subi;
    int                    index_int;
    unsigned               circuit_index;
#if RNET_SERVER_MODE_LOOPBACK == 0
    rnet_cir_ram_t        *circuit_ptr;
#endif

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of UDP header
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Sanity check
    if (buf->header.length < UDP_HEADER_SIZE)
    {
        buf->header.code = RNET_BUF_CODE_UDP_PACKET_TOO_SMALL;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    udp_deserialize_header(&header, ptr);

    is_ipv6 = RNET_PH_IPV6 == buf->header.previous_ph;

    // Get pointer to source IP address in IP header
    if (is_ipv6)
    {
        src_ip_addr_ptr = &buf->buf[buf->header.offset - IPV6_HEADER_SIZE
                                    + IPV6_SRC_ADDR_OFFSET];
    }
    else
    {
        src_ip_addr_ptr = &buf->buf[buf->header.offset - IPV4_HEADER_SIZE
                                     +IPV4_SRC_ADDR_OFFSET];
    }

    // Remove UDP header. 'offset'+'length' defines payload now.
    buf->header.offset += UDP_HEADER_SIZE;
    buf->header.length -= UDP_HEADER_SIZE;

    // sanity check that header offset+length don't overrrun RNET buffer
    if (buf->header.offset + buf->header.length > RNET_BUF_SIZE)
    {
        buf->header.code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    // Match subinterface.
    subi = buf->header.subi;

    // Look up circuit
    // Ignore source port during lookup
    index_int = rnet_circuit_index_lookup(subi,
                              RNET_IP_PROTOCOL_UDP,
                              header.destination_port,
                              0,
                              (rnet_ip_addr_union_t *)src_ip_addr_ptr);
    if (RFAIL_NOT_FOUND == index_int)
    {
        buf->header.code = RNET_BUF_CODE_UDP_CIRCUIT_NOT_FOUND;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    circuit_index = (unsigned)index_int;
    buf->header.circuit = circuit_index;
    buf->header.previous_ph = RNET_PH_UDP;

#if RNET_SERVER_MODE_LOOPBACK == 0
    circuit_ptr = rnet_circuit_get(circuit_index);

    // Is listener registered with this circuit? Send packet her way then.
    if (RNET_LISTENER_MSG_DISABLED != circuit_ptr->buf_listener_msg)
    {
        rnet_msg_send_buf_to_listener(circuit_ptr->buf_listener_msg,
                                      circuit_ptr->listener_task,
                                      buf);
    }
    // Nobody's interested. Drop it.
    else
    {
        rnet_msg_buf_discard(buf);
    }
#else
    rnet_msg_send(RNET_ID_TX_BUF_UDP, buf);
#endif
}

//!
//! @name      rnet_msg_rx_buf_udp
//!
//! @brief     Entry point for UDP packet
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_rx_pcl_udp(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t     *pcl_header;
    uint8_t               *ptr;
    uint8_t               *src_ip_addr_ptr;
    bool                   is_ipv6;
    rnet_udp_header_t      header;
    rnet_subi_t            subi;
    int                    index_int;
    unsigned               circuit_index;
#if RNET_SERVER_MODE_LOOPBACK == 0
    rnet_cir_ram_t        *circuit_ptr;
#endif

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    // 'ptr' points to beginning of IP header
    pcl_header = NSVC_PCL_HEADER(head_pcl);
    ptr = &head_pcl->buffer[pcl_header->offset];

    // Sanity check
    if (pcl_header->total_used_length < UDP_HEADER_SIZE)
    {
        pcl_header->code = RNET_BUF_CODE_UDP_PACKET_TOO_SMALL;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    udp_deserialize_header(&header, ptr);

    is_ipv6 = RNET_PH_IPV6 == pcl_header->previous_ph;

    // Get pointer to source IP address in IP header
    if (is_ipv6)
    {
        src_ip_addr_ptr = 
         &head_pcl->buffer[pcl_header->offset - IPV6_HEADER_SIZE + IPV6_SRC_ADDR_OFFSET];
    }
    else
    {
        src_ip_addr_ptr = 
         &head_pcl->buffer[pcl_header->offset - IPV4_HEADER_SIZE + IPV4_SRC_ADDR_OFFSET];
    }

    // Remove UDP header. 'offset'+'length' defines payload now.
    pcl_header->offset += UDP_HEADER_SIZE;
    pcl_header->total_used_length -= UDP_HEADER_SIZE;

    // sanity check that header offset+length don't overrrun RNET buffer
    if (pcl_header->offset + pcl_header->total_used_length > RNET_BUF_SIZE)
    {
        pcl_header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    // Match subinterface.
    subi = pcl_header->subi;

    // Look up circuit
    // Ignore source port during lookup
    index_int = rnet_circuit_index_lookup(subi,
                              RNET_IP_PROTOCOL_UDP,
                              header.destination_port,
                              0,
                              (rnet_ip_addr_union_t *)src_ip_addr_ptr);
    if (RFAIL_NOT_FOUND == index_int)
    {
        pcl_header->code = RNET_BUF_CODE_UDP_CIRCUIT_NOT_FOUND;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    circuit_index = (unsigned)index_int;
    pcl_header->circuit = circuit_index;
    pcl_header->previous_ph = RNET_PH_UDP;

#if RNET_SERVER_MODE_LOOPBACK == 0
    circuit_ptr = rnet_circuit_get(circuit_index);

    // Is listener registered with this circuit? Send packet her way then.
    if (RNET_LISTENER_MSG_DISABLED != circuit_ptr->pcl_listener_msg)
    {
        rnet_msg_send_pcl_to_listener(circuit_ptr->pcl_listener_msg,
                                      circuit_ptr->listener_task,
                                      head_pcl);
    }
    // Nobody's interested. Drop it.
    else
    {
        rnet_msg_pcl_discard(head_pcl);
    }
#else
    rnet_msg_send(RNET_ID_TX_PCL_UDP, head_pcl);
#endif
}

//!
//! @name      rnet_msg_tx_buf_udp
//!
//! @brief     Send vector for UDP packets, from application layer
//!
//! @param[in] 'buf'--
//!
void rnet_msg_tx_buf_udp(rnet_buf_t *buf)
{
    uint8_t               *ptr;
    rnet_udp_header_t      header;
    rnet_udp_header_t      swap_header;
    unsigned               circuit_index;
    rnet_cir_ram_t        *circuit_ptr;

    SL_REQUIRE(IS_RNET_BUF(buf));

    circuit_index = buf->header.circuit;
    circuit_ptr = rnet_circuit_get(circuit_index);

    // Sanity checks
    if ((buf->header.offset < UDP_HEADER_SIZE)  ||
        (NULL == circuit_ptr)                   ||
        (buf->header.offset + buf->header.length > RNET_BUF_SIZE))
    {
        buf->header.code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    // Allocate space for UDP header
    buf->header.previous_ph = RNET_PH_UDP;
    buf->header.offset -= UDP_HEADER_SIZE;
    buf->header.length += UDP_HEADER_SIZE;

    // 'ptr' points to beginning of UDP header
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Fill in header.
    // Must set checksum to zero for L4 checksum calc in IP layer.
    header.source_port = circuit_ptr->self_port;
    if (0 == circuit_ptr->peer_port)
    {
        // If circuit is set up to act in server mode, most use
        // client's source port as destination port. Assume client
        // hasn't tampered with packet.
        udp_deserialize_header(&swap_header, ptr);
        header.destination_port = swap_header.source_port;

        // Server mode: tell L3 tx to swap source and dest IP addresses
        buf->header.circuit = RNET_CIR_INDEX_SWAP_SRC_DEST;
    }
    else
    {
        header.destination_port = circuit_ptr->peer_port;
    }
    header.length = buf->header.length + UDP_HEADER_SIZE;
    header.checksum = 0;

    // Write UDP header
    udp_serialize_header(ptr, &header);

    // Send packet on its way
    if (rnet_circuit_is_ipv4(circuit_index))
    {
        rnet_msg_send(RNET_ID_TX_BUF_IPV4, buf);
    }
    else if (rnet_circuit_is_ipv6(circuit_index))
    {
        rnet_msg_send(RNET_ID_TX_BUF_IPV6, buf);
    }
    else
    {
        buf->header.code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_tx_pcl_udp
//!
//! @brief     Send vector for UDP packets, from application layer
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_tx_pcl_udp(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t     *pcl_header;
    uint8_t               *ptr;
    rnet_udp_header_t      header;
    rnet_udp_header_t      swap_header;
    unsigned               chain_capacity;
    unsigned               circuit_index;
    rnet_cir_ram_t        *circuit_ptr;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    pcl_header = NSVC_PCL_HEADER(head_pcl);
    circuit_index = pcl_header->circuit;
    circuit_ptr = rnet_circuit_get(circuit_index);

    // Sanity checks
    //  1. 'offset' leaves enough room to pre-pend UDP header
    //  2. circuit is valid
    //  3. 'offset' is on first pcl (all network headers on 1st pcl)
    //  4. pcl header indicates length which doesn't exceed
    //     storage capability of pcl.
    chain_capacity = nsvc_pcl_chain_capacity(
                        nsvc_pcl_count_pcls_in_chain(head_pcl), true);
    if (
        (pcl_header->offset < UDP_HEADER_SIZE)
                  ||
        (NULL == circuit_ptr)
                  ||
        (pcl_header->offset >= NSVC_PCL_SIZE)
                  ||
        ((unsigned)(pcl_header->offset + pcl_header->total_used_length) >
                                  chain_capacity)
       )

    {
        pcl_header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    // Allocate space for UDP header
    pcl_header->previous_ph = RNET_PH_UDP;
    pcl_header->offset -= UDP_HEADER_SIZE;
    pcl_header->total_used_length += UDP_HEADER_SIZE;

    // 'ptr' points to beginning of UDP header
    ptr = &head_pcl->buffer[pcl_header->offset];

    // Fill in header.
    // Must set checksum to zero for L4 checksum calc in IP layer.
    header.source_port = circuit_ptr->self_port;
    if (0 == circuit_ptr->peer_port)
    {
        // If circuit is set up to act in server mode, most use
        // client's source port as destination port. Assume client
        // hasn't tampered with packet.
        udp_deserialize_header(&swap_header, ptr);
        header.destination_port = swap_header.source_port;
    }
    else
    {
        header.destination_port = circuit_ptr->peer_port;
    }
    header.length = pcl_header->total_used_length + UDP_HEADER_SIZE;
    header.checksum = 0;

    // Write UDP header
    udp_serialize_header(ptr, &header);

    // Send packet on its way
    if (rnet_circuit_is_ipv4(circuit_index))
    {
        rnet_msg_send(RNET_ID_TX_PCL_IPV4, head_pcl);
    }
    else if (rnet_circuit_is_ipv6(circuit_index))
    {
        rnet_msg_send(RNET_ID_TX_PCL_IPV6, head_pcl);
    }
    else
    {
        pcl_header->code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}

//!
//! @name      udp_serialize_header
//!
//! @brief     UDP header struct to byte stream
//!
//! @param[out] 'buffer'-- byte stream
//! @param[in] 'header'-- source info
//!
void udp_serialize_header(uint8_t                 *buffer,
                          const rnet_udp_header_t *header)
{
    rutils_word16_to_stream(buffer, header->source_port);
    buffer += BYTES_PER_WORD16;

    rutils_word16_to_stream(buffer, header->destination_port);
    buffer += BYTES_PER_WORD16;

    rutils_word16_to_stream(buffer, header->length);
    buffer += BYTES_PER_WORD16;

    rutils_word16_to_stream(buffer, header->checksum);
    buffer += BYTES_PER_WORD16;
}

//!
//! @name      udp_deserialize_header
//!
//! @brief     scan UDP header struct from byte stream
//!
//! @param[out] 'header'-- fill in
//! @param[in] 'buffer'-- scan starting here
//!
void udp_deserialize_header(rnet_udp_header_t *header,
                            uint8_t           *buffer)
{
    header->source_port = rutils_stream_to_word16(buffer);
    buffer += BYTES_PER_WORD16;

    header->destination_port = rutils_stream_to_word16(buffer);
    buffer += BYTES_PER_WORD16;

    header->length = rutils_stream_to_word16(buffer);
    buffer += BYTES_PER_WORD16;

    header->checksum = rutils_stream_to_word16(buffer);
    buffer += BYTES_PER_WORD16;
}